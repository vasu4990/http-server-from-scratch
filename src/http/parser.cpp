#include "vhttp/http/parser.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <sstream>
#include <utility>

namespace vhttp::http {
namespace {

std::string ascii_lower(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (const unsigned char ch : input) {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

std::string_view trim_ows(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

bool valid_token(std::string_view value) {
    if (value.empty()) {
        return false;
    }

    constexpr std::string_view separators = "()<>@,;:\\\"/[]?={} \t";
    for (const unsigned char ch : value) {
        if (ch <= 0x20U || ch >= 0x7FU || separators.find(static_cast<char>(ch)) != std::string_view::npos) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::string_view HttpRequest::header(std::string_view name) const {
    const auto key = ascii_lower(name);
    const auto it = headers.find(key);
    if (it == headers.end()) {
        return {};
    }
    return it->second;
}

HttpRequestParser::HttpRequestParser(ParserLimits limits) : limits_(limits) {}

ParseStatus HttpRequestParser::feed(std::string_view bytes) {
    if (state_ == State::complete) {
        buffer_.append(bytes.data(), bytes.size());
        return ParseStatus::complete;
    }
    if (state_ == State::error) {
        return ParseStatus::error;
    }

    buffer_.append(bytes.data(), bytes.size());
    return parse_available();
}

ParseStatus HttpRequestParser::parse_available() {
    while (true) {
        if (state_ == State::request_line || state_ == State::headers) {
            const auto pos = buffer_.find("\r\n");
            if (pos == std::string::npos) {
                if (state_ == State::request_line && buffer_.size() > limits_.max_request_line_bytes) {
                    fail("request line exceeds configured limit");
                    return ParseStatus::error;
                }
                if (state_ == State::headers && header_bytes_ + buffer_.size() > limits_.max_header_bytes) {
                    fail("headers exceed configured byte limit");
                    return ParseStatus::error;
                }
                return ParseStatus::need_more_data;
            }

            const std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);

            if (state_ == State::request_line) {
                if (line.size() > limits_.max_request_line_bytes || !parse_request_line(line)) {
                    return ParseStatus::error;
                }
                state_ = State::headers;
                continue;
            }

            header_bytes_ += line.size() + 2;
            if (header_bytes_ > limits_.max_header_bytes) {
                fail("headers exceed configured byte limit");
                return ParseStatus::error;
            }

            if (line.empty()) {
                if (!finalize_headers()) {
                    return ParseStatus::error;
                }
                if (!content_length_.has_value() || *content_length_ == 0) {
                    state_ = State::complete;
                    return ParseStatus::complete;
                }
                state_ = State::body;
                continue;
            }

            if (++header_count_ > limits_.max_header_count) {
                fail("header count exceeds configured limit");
                return ParseStatus::error;
            }
            if (!parse_header_line(line)) {
                return ParseStatus::error;
            }
            continue;
        }

        if (state_ == State::body) {
            const auto needed = *content_length_ - request_.body.size();
            const auto take = std::min(needed, buffer_.size());
            request_.body.append(buffer_.data(), take);
            buffer_.erase(0, take);

            if (request_.body.size() == *content_length_) {
                state_ = State::complete;
                return ParseStatus::complete;
            }
            return ParseStatus::need_more_data;
        }

        return state_ == State::complete ? ParseStatus::complete : ParseStatus::error;
    }
}

bool HttpRequestParser::parse_request_line(std::string_view line) {
    const auto first_space = line.find(' ');
    if (first_space == std::string_view::npos) {
        fail("malformed request line: missing method separator");
        return false;
    }
    const auto second_space = line.find(' ', first_space + 1);
    if (second_space == std::string_view::npos || line.find(' ', second_space + 1) != std::string_view::npos) {
        fail("malformed request line: expected METHOD SP TARGET SP VERSION");
        return false;
    }

    const auto method = line.substr(0, first_space);
    const auto target = line.substr(first_space + 1, second_space - first_space - 1);
    const auto version = line.substr(second_space + 1);

    if (!valid_token(method)) {
        fail("invalid HTTP method token");
        return false;
    }
    if (target.empty()) {
        fail("empty request target");
        return false;
    }
    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        fail("unsupported HTTP version");
        return false;
    }

    request_.method.assign(method);
    request_.target.assign(target);
    request_.version.assign(version);
    return true;
}

bool HttpRequestParser::parse_header_line(std::string_view line) {
    const auto colon = line.find(':');
    if (colon == std::string_view::npos) {
        fail("malformed header: missing colon");
        return false;
    }

    const auto raw_name = line.substr(0, colon);
    if (!valid_token(raw_name)) {
        fail("invalid header name");
        return false;
    }

    const std::string name = ascii_lower(raw_name);
    const std::string value(trim_ows(line.substr(colon + 1)));

    auto [it, inserted] = request_.headers.emplace(name, value);
    if (!inserted) {
        if (name == "content-length" && it->second != value) {
            fail("conflicting Content-Length headers");
            return false;
        }
        // RFC field combination rules vary by field. For this initial milestone,
        // preserve repeated non-framing fields as a comma-separated value.
        if (name != "content-length") {
            it->second.append(", ");
            it->second.append(value);
        }
    }
    return true;
}

bool HttpRequestParser::finalize_headers() {
    if (const auto te = request_.headers.find("transfer-encoding"); te != request_.headers.end()) {
        // Chunked decoding is deliberately deferred to the next milestone.
        fail("Transfer-Encoding is not supported in milestone 1");
        return false;
    }

    const auto it = request_.headers.find("content-length");
    if (it == request_.headers.end()) {
        return true;
    }

    std::size_t parsed = 0;
    const auto begin = it->second.data();
    const auto end = begin + it->second.size();
    auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end) {
        fail("invalid Content-Length");
        return false;
    }
    if (parsed > limits_.max_body_bytes) {
        fail("request body exceeds configured limit");
        return false;
    }
    content_length_ = parsed;
    return true;
}

void HttpRequestParser::fail(std::string message) {
    state_ = State::error;
    error_message_ = std::move(message);
}

std::string HttpRequestParser::take_remaining() {
    std::string out = std::move(buffer_);
    buffer_.clear();
    return out;
}

void HttpRequestParser::reset() {
    state_ = State::request_line;
    request_ = {};
    buffer_.clear();
    error_message_.clear();
    header_bytes_ = 0;
    header_count_ = 0;
    content_length_.reset();
}

}  // namespace vhttp::http
