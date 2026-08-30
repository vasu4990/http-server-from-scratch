#include "vhttp/http/response.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    auto response = vhttp::http::HttpResponse::text(200, "OK", "hello");
    const std::string wire = response.serialize(false);

    expect(wire.starts_with("HTTP/1.1 200 OK\r\n"), "status line emitted");
    expect(wire.find("Content-Length: 5\r\n") != std::string::npos, "content length generated");
    expect(wire.find("Connection: close\r\n") != std::string::npos, "connection close generated");
    expect(wire.ends_with("\r\n\r\nhello"), "body serialized");

    std::cout << "response tests passed\n";
}
