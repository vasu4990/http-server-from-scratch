#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace vhttp::http {

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    [[nodiscard]] std::string_view header(std::string_view name) const;
};

}  // namespace vhttp::http
