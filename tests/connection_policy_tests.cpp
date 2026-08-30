#include "vhttp/server/connection.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

vhttp::http::HttpRequest request(std::string version, std::string connection = {}) {
    vhttp::http::HttpRequest value;
    value.version = std::move(version);
    if (!connection.empty()) {
        value.headers.emplace("connection", std::move(connection));
    }
    return value;
}

void http11_is_persistent_by_default() {
    expect(vhttp::server::request_allows_persistent_connection(request("HTTP/1.1")),
           "HTTP/1.1 should default to persistent");
}

void http11_close_token_wins() {
    expect(!vhttp::server::request_allows_persistent_connection(
               request("HTTP/1.1", "keep-alive, CLOSE")),
           "HTTP/1.1 close token should disable persistence");
}

void http10_is_nonpersistent_by_default() {
    expect(!vhttp::server::request_allows_persistent_connection(request("HTTP/1.0")),
           "HTTP/1.0 should default to close");
}

void http10_can_opt_into_keepalive() {
    expect(vhttp::server::request_allows_persistent_connection(
               request("HTTP/1.0", "Keep-Alive")),
           "HTTP/1.0 keep-alive token should enable persistence");
}

void response_can_force_close() {
    vhttp::http::HttpResponse response;
    response.headers.emplace("cOnNeCtIoN", "close");
    expect(vhttp::server::response_requests_connection_close(response),
           "response Connection close should be case-insensitive");
}

void token_parser_respects_comma_boundaries() {
    expect(vhttp::server::header_value_has_token("upgrade, keep-alive", "KEEP-ALIVE"),
           "comma separated token should match case-insensitively");
    expect(!vhttp::server::header_value_has_token("xkeep-alive", "keep-alive"),
           "substring must not count as a connection token");
}

}  // namespace

int main() {
    http11_is_persistent_by_default();
    http11_close_token_wins();
    http10_is_nonpersistent_by_default();
    http10_can_opt_into_keepalive();
    response_can_force_close();
    token_parser_respects_comma_boundaries();
    std::cout << "connection policy tests passed\n";
}
