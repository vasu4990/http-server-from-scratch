#include "vhttp/router/router.hpp"
#include "vhttp/server/server.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <string>

int main(int argc, char** argv) {
    try {
        std::uint16_t port = 8080;
        if (argc > 1) {
            const unsigned long parsed = std::stoul(argv[1]);
            if (parsed == 0 || parsed > std::numeric_limits<std::uint16_t>::max()) {
                throw std::out_of_range("port must be between 1 and 65535");
            }
            port = static_cast<std::uint16_t>(parsed);
        }

        vhttp::router::Router router;
        router.get("/", [](const auto&) {
            return vhttp::http::HttpResponse::text(
                200,
                "OK",
                "Hello from vhttp. This response came from a C++20 HTTP server built from sockets.\n");
        });
        router.get("/health", [](const auto&) {
            return vhttp::http::HttpResponse::text(200, "OK", "healthy\n");
        });
        router.get("/users/:id", [](const auto& request) {
            return vhttp::http::HttpResponse::text(
                200, "OK", "user=" + std::string(request.path_param("id")) + "\n");
        });

        vhttp::server::Server server([&router](const auto& request) { return router.dispatch(request); });
        server.listen_and_serve("0.0.0.0", port);
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << '\n';
        return 1;
    }
}
