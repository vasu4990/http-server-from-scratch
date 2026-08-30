#include "vhttp/server/connection.hpp"

#include <cctype>
#include <string>

namespace vhttp::server {
namespace {

char ascii_lower_char(char value) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

bool ascii_iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (ascii_lower_char(lhs[i]) != ascii_lower_char(rhs[i])) {
            return false;
        }
    }
    return true;
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

}  // namespace

bool header_value_has_token(std::string_view value, std::string_view token) {
    while (!value.empty()) {
        const auto comma = value.find(',');
        const auto current = trim_ows(value.substr(0, comma));
        if (ascii_iequals(current, token)) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        value.remove_prefix(comma + 1);
    }
    return false;
}

bool request_allows_persistent_connection(const http::HttpRequest& request) {
    const auto connection = request.header("connection");
    if (header_value_has_token(connection, "close")) {
        return false;
    }

    if (request.version == "HTTP/1.1") {
        return true;
    }
    if (request.version == "HTTP/1.0") {
        return header_value_has_token(connection, "keep-alive");
    }
    return false;
}

bool response_requests_connection_close(const http::HttpResponse& response) {
    for (const auto& [name, value] : response.headers) {
        if (ascii_iequals(name, "connection") && header_value_has_token(value, "close")) {
            return true;
        }
    }
    return false;
}

}  // namespace vhttp::server
