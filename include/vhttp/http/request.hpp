#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vhttp::http {

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    // Populated by the router during dispatch. The parser intentionally keeps
    // the raw request-target intact so transport parsing and routing remain
    // separate concerns.
    std::string path;
    std::unordered_map<std::string, std::string> path_params;
    std::unordered_map<std::string, std::vector<std::string>> query_params;

    [[nodiscard]] std::string_view header(std::string_view name) const;
    [[nodiscard]] std::string_view path_param(std::string_view name) const;
    [[nodiscard]] std::string_view query(std::string_view name) const;
};

}  // namespace vhttp::http
