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

std::string HttpResponse::serialize(bool keep_alive, bool suppress_body) const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << ' ' << reason << "\r\n";

    bool has_content_length = false;
    for (const auto& [name, value] : headers) {
        if (ascii_iequals(name, "content-length")) {
            has_content_length = true;
        }
        if (ascii_iequals(name, "connection")) {
            // Connection lifetime is a transport decision. Handlers may request
            // closure, but serialization emits the final server decision exactly
            // once so a max-request or timeout policy cannot be contradicted.
            continue;
        }
        out << name << ": " << value << "\r\n";
    }

    if (!has_content_length) {
        out << "Content-Length: " << body.size() << "\r\n";
    }
    out << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    out << "\r\n";
    if (!suppress_body) {
        out << body;
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
