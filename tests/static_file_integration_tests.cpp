#include "vhttp/net/socket.hpp"
#include "vhttp/server/server.hpp"
#include "vhttp/static_files/static_file_handler.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

class TempRoot {
public:
    TempRoot() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("vhttp-static-tcp-" + std::to_string(nonce));
        std::filesystem::create_directories(path);
        std::ofstream out(path / "asset.txt", std::ios::binary);
        out << "0123456789";
    }

    ~TempRoot() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
};

std::string receive_until_close(vhttp::net::TcpStream& stream) {
    std::array<char, 4096> buffer{};
    std::string result;
    while (true) {
        const auto received = stream.receive(buffer.data(), buffer.size());
        if (received == 0) {
            break;
        }
        result.append(buffer.data(), static_cast<std::size_t>(received));
    }
    return result;
}

std::size_t occurrences(std::string_view haystack, std::string_view needle) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = haystack.find(needle, position)) != std::string_view::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

void exercises_static_get_head_conditionals_and_ranges_over_tcp() {
    TempRoot root;
    vhttp::static_files::StaticFileConfig static_config;
    static_config.document_root = root.path;
    static_config.url_prefix = "/static";
    vhttp::static_files::StaticFileHandler files(std::move(static_config));

    vhttp::http::HttpRequest metadata_request;
    metadata_request.method = "GET";
    metadata_request.target = "/static/asset.txt";
    metadata_request.version = "HTTP/1.1";
    const auto metadata = files.try_serve(metadata_request);
    expect(metadata && metadata->status == 200, "metadata fixture request should resolve");
    const auto etag = metadata->headers.at("ETag");

    auto listener = vhttp::net::TcpListener::bind("127.0.0.1", 0);
    const auto port = listener.local_port();

    vhttp::server::ConnectionConfig connection_config;
    connection_config.max_requests_per_connection = 10;
    connection_config.idle_timeout = std::chrono::milliseconds(2000);

    vhttp::server::Server server(
        [&files](const vhttp::http::HttpRequest& incoming) {
            if (auto response = files.try_serve(incoming)) {
                return *response;
            }
            return vhttp::http::HttpResponse::text(404, "Not Found", "Not Found\n");
        },
        connection_config);

    std::thread worker([&] { server.serve_connection(listener.accept()); });
    auto client = vhttp::net::TcpStream::connect("127.0.0.1", port);
    client.set_receive_timeout(std::chrono::milliseconds(3000));

    const std::string requests =
        "GET /static/asset.txt HTTP/1.1\r\nHost: localhost\r\n\r\n"
        "HEAD /static/asset.txt HTTP/1.1\r\nHost: localhost\r\n\r\n"
        "GET /static/asset.txt HTTP/1.1\r\nHost: localhost\r\nRange: bytes=2-5\r\n\r\n"
        "GET /static/asset.txt HTTP/1.1\r\nHost: localhost\r\nIf-None-Match: " + etag + "\r\n\r\n"
        "GET /static/asset.txt HTTP/1.1\r\nHost: localhost\r\nRange: bytes=99-100\r\nConnection: close\r\n\r\n";
    client.send_all(requests);

    const auto wire = receive_until_close(client);
    worker.join();

    expect(occurrences(wire, "HTTP/1.1 200 OK") == 2, "GET and HEAD should both return 200");
    expect(occurrences(wire, "HTTP/1.1 206 Partial Content") == 1, "satisfiable range should return 206");
    expect(occurrences(wire, "HTTP/1.1 304 Not Modified") == 1, "matching ETag should return 304");
    expect(occurrences(wire, "HTTP/1.1 416 Range Not Satisfiable") == 1, "unsatisfiable range should return 416");
    expect(wire.find("Content-Range: bytes 2-5/10\r\n") != std::string::npos,
           "206 response should carry the selected range");
    expect(wire.find("Content-Range: bytes */10\r\n") != std::string::npos,
           "416 response should carry the current complete length");
    expect(occurrences(wire, "0123456789") == 1,
           "HEAD and 304 must not duplicate the complete file body on the wire");
    expect(wire.find("2345") != std::string::npos, "206 response body should contain the selected bytes");
}

}  // namespace

int main() {
    vhttp::net::SocketRuntime runtime;
    exercises_static_get_head_conditionals_and_ranges_over_tcp();
    std::cout << "static file integration tests passed\n";
}
