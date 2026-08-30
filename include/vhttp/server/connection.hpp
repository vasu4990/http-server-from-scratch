#pragma once

#include "vhttp/http/request.hpp"
#include "vhttp/http/response.hpp"

#include <chrono>
#include <cstddef>
#include <string_view>

namespace vhttp::server {

struct ConnectionConfig {
    std::size_t max_requests_per_connection = 100;
    std::chrono::milliseconds idle_timeout{15000};
};

[[nodiscard]] bool header_value_has_token(std::string_view value, std::string_view token);
[[nodiscard]] bool request_allows_persistent_connection(const http::HttpRequest& request);
[[nodiscard]] bool response_requests_connection_close(const http::HttpResponse& response);

}  // namespace vhttp::server
