#include "vhttp/router/router.hpp"
#include "vhttp/server/server.hpp"
#include "vhttp/static_files/static_file_handler.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <utility>

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

        std::optional<vhttp::static_files::StaticFileHandler> static_files;
        if (argc > 2) {
            vhttp::static_files::StaticFileConfig config;
            config.document_root = argv[2];
            config.url_prefix = "/static";
            static_files.emplace(std::move(config));
            std::cout << "serving static files from " << static_files->document_root().string()
                      << " under /static\n";
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
        router.post("/echo", [](const auto& request) {
            auto response = vhttp::http::HttpResponse::text(200, "OK", request.body);
            if (const auto checksum = request.trailer("x-checksum"); !checksum.empty()) {
                response.set_header("X-Received-Checksum", std::string(checksum));
            }
            return response;
        });
        router.get("/chunked", [](const auto&) {
            auto response = vhttp::http::HttpResponse::text(
                200, "OK", "This response is framed with Transfer-Encoding: chunked.\n");
            response.set_chunked();
            return response;
        });

        vhttp::server::Server server([&](const auto& request) {
            if (static_files) {
                if (auto response = static_files->try_serve(request)) {
                    return *response;
                }
            }
            return router.dispatch(request);
        });
        server.listen_and_serve("0.0.0.0", port);
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << '\n';
        return 1;
    }
}
