#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace vhttp::http {

struct HttpResponse {
    int status = 200;
    std::string reason = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    HttpResponse& set_status(int code, std::string text);
    HttpResponse& set_header(std::string name, std::string value);
    HttpResponse& set_body(std::string value);

    [[nodiscard]] std::string serialize(bool keep_alive = false) const;

    static HttpResponse text(int status, std::string reason, std::string body,
                             std::string content_type = "text/plain; charset=utf-8");
};

}  // namespace vhttp::http
