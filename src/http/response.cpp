#include "vhttp/http/response.hpp"

#include <sstream>
#include <utility>

namespace vhttp::http {

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

std::string HttpResponse::serialize(bool keep_alive) const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << ' ' << reason << "\r\n";

    bool has_content_length = false;
    bool has_connection = false;
    for (const auto& [name, value] : headers) {
        if (name == "Content-Length" || name == "content-length") {
            has_content_length = true;
        }
        if (name == "Connection" || name == "connection") {
            has_connection = true;
        }
        out << name << ": " << value << "\r\n";
    }

    if (!has_content_length) {
        out << "Content-Length: " << body.size() << "\r\n";
    }
    if (!has_connection) {
        out << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    }
    out << "\r\n";
    out << body;
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
