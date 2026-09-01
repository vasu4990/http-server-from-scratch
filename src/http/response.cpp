#include "vhttp/http/response.hpp"

#include <cctype>
#include <sstream>
#include <utility>

namespace vhttp::http {
namespace {

bool ascii_iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const auto a = static_cast<char>(std::tolower(static_cast<unsigned char>(lhs[i])));
        const auto b = static_cast<char>(std::tolower(static_cast<unsigned char>(rhs[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

std::string chunked_body(std::string_view body) {
    if (body.empty()) {
        return "0\r\n\r\n";
    }

    std::ostringstream out;
    out << std::hex << body.size() << "\r\n";
    out << body << "\r\n0\r\n\r\n";
    return out.str();
}

bool status_forbids_message_body(int status) {
    return (status >= 100 && status < 200) || status == 204 || status == 304;
}

}  // namespace

HttpResponse& HttpResponse::set_status(int code, std::string text) {
    status = code;
    reason = std::move(text);
    return *this;
}

HttpResponse& HttpResponse::set_header(std::string name, std::string value) {
    headers[std::move(name)] = std::move(value);
    return *this;
}

HttpResponse& HttpResponse::set_body(std::string value) {
    body = std::move(value);
    return *this;
}

HttpResponse& HttpResponse::set_chunked(bool enabled) noexcept {
    chunked = enabled;
    return *this;
}

HttpResponse& HttpResponse::set_suppressed_body_length(std::size_t length) noexcept {
    suppressed_body_length = length;
    return *this;
}

std::string HttpResponse::serialize(bool keep_alive, bool suppress_body) const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << ' ' << reason << "\r\n";

    const bool body_forbidden = status_forbids_message_body(status);
    for (const auto& [name, value] : headers) {
        if (ascii_iequals(name, "content-length") || ascii_iequals(name, "transfer-encoding")) {
            // Message framing is emitted from the actual body/encoder below.
            // A handler cannot inject contradictory length or transfer metadata.
            continue;
        }
        if (ascii_iequals(name, "connection")) {
            // Connection lifetime is a transport decision. Handlers may request
            // closure, but serialization emits the final server decision exactly
            // once so a max-request or timeout policy cannot be contradicted.
            continue;
        }
        out << name << ": " << value << "\r\n";
    }

    if (!body_forbidden) {
        if (chunked) {
            out << "Transfer-Encoding: chunked\r\n";
        } else {
            const auto content_length =
                suppress_body && suppressed_body_length.has_value() ? *suppressed_body_length : body.size();
            out << "Content-Length: " << content_length << "\r\n";
        }
    }
    out << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    out << "\r\n";

    if (!suppress_body && !body_forbidden) {
        if (chunked) {
            out << chunked_body(body);
        } else {
            out << body;
        }
    }
    return out.str();
}

HttpResponse HttpResponse::text(int code, std::string reason, std::string body,
                                std::string content_type) {
    HttpResponse response;
    response.status = code;
    response.reason = std::move(reason);
    response.headers.emplace("Content-Type", std::move(content_type));
    response.body = std::move(body);
    return response;
}

}  // namespace vhttp::http
