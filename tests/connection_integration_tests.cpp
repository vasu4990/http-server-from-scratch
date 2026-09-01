#include "vhttp/net/socket.hpp"
#include "vhttp/server/server.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

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

void serves_two_pipelined_requests_on_one_connection() {
    auto listener = vhttp::net::TcpListener::bind("127.0.0.1", 0);
    const auto port = listener.local_port();

    vhttp::server::ConnectionConfig config;
    config.max_requests_per_connection = 10;
    config.idle_timeout = std::chrono::milliseconds(2000);

    vhttp::server::Server server(
        [](const vhttp::http::HttpRequest& request) {
            return vhttp::http::HttpResponse::text(200, "OK", request.target + "\n");
        },
        config);

    std::thread worker([&] { server.serve_connection(listener.accept()); });
    auto client = vhttp::net::TcpStream::connect("127.0.0.1", port);
    client.set_receive_timeout(std::chrono::milliseconds(3000));

    client.send_all(
        "GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n"
        "GET /second HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

    const auto wire = receive_until_close(client);
    worker.join();

    expect(occurrences(wire, "HTTP/1.1 200 OK") == 2,
           "two pipelined requests should produce two ordered responses");
    expect(wire.find("Connection: keep-alive") != std::string::npos,
           "first HTTP/1.1 response should keep the socket alive");
    expect(wire.find("Connection: close") != std::string::npos,
           "second response should advertise closure");
    expect(wire.find("/first\n") < wire.find("/second\n"),
           "pipelined responses should preserve request order");
}

void chunked_request_can_be_followed_by_pipelined_request() {
    auto listener = vhttp::net::TcpListener::bind("127.0.0.1", 0);
    const auto port = listener.local_port();

    vhttp::server::ConnectionConfig config;
    config.max_requests_per_connection = 10;
    config.idle_timeout = std::chrono::milliseconds(2000);

    vhttp::server::Server server(
        [](const vhttp::http::HttpRequest& request) {
            if (request.target == "/chunked") {
                auto response = vhttp::http::HttpResponse::text(
                    200, "OK", request.body + "|" + std::string(request.trailer("x-checksum")));
                response.set_chunked();
                return response;
            }
            return vhttp::http::HttpResponse::text(200, "OK", "after\n");
        },
        config);

    std::thread worker([&] { server.serve_connection(listener.accept()); });
    auto client = vhttp::net::TcpStream::connect("127.0.0.1", port);
    client.set_receive_timeout(std::chrono::milliseconds(3000));

    client.send_all(
        "POST /chunked HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n"
        "Trailer: X-Checksum\r\n\r\n"
        "4\r\nWiki\r\n5\r\npedia\r\n0\r\nX-Checksum: abc\r\n\r\n"
        "GET /after HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

    const auto wire = receive_until_close(client);
    worker.join();

    expect(occurrences(wire, "HTTP/1.1 200 OK") == 2,
           "chunked request plus pipelined request should both be dispatched");
    expect(wire.find("Transfer-Encoding: chunked\r\n") != std::string::npos,
           "first response should exercise chunked response encoder");
    expect(wire.find("d\r\nWikipedia|abc\r\n0\r\n\r\n") != std::string::npos,
           "handler should receive decoded body and trailer before chunked response encoding");
    expect(wire.find("after\n") != std::string::npos,
           "bytes after terminal chunk/trailers should survive into next request");
}

void max_request_limit_forces_close() {
    auto listener = vhttp::net::TcpListener::bind("127.0.0.1", 0);
    const auto port = listener.local_port();

    vhttp::server::ConnectionConfig config;
    config.max_requests_per_connection = 1;
    config.idle_timeout = std::chrono::milliseconds(2000);

    vhttp::server::Server server(
        [](const vhttp::http::HttpRequest&) {
            return vhttp::http::HttpResponse::text(200, "OK", "limited\n");
        },
        config);

    std::thread worker([&] { server.serve_connection(listener.accept()); });
    auto client = vhttp::net::TcpStream::connect("127.0.0.1", port);
    client.set_receive_timeout(std::chrono::milliseconds(3000));
    client.send_all("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");

    const auto wire = receive_until_close(client);
    worker.join();

    expect(occurrences(wire, "HTTP/1.1 200 OK") == 1, "request limit should still serve first request");
    expect(wire.find("Connection: close") != std::string::npos,
           "request limit should force an explicit close response");
}

void idle_timeout_retires_silent_connection() {
    auto listener = vhttp::net::TcpListener::bind("127.0.0.1", 0);
    const auto port = listener.local_port();

    vhttp::server::ConnectionConfig config;
    config.max_requests_per_connection = 10;
    config.idle_timeout = std::chrono::milliseconds(250);

    vhttp::server::Server server(
        [](const vhttp::http::HttpRequest&) {
            return vhttp::http::HttpResponse::text(200, "OK", "unexpected\n");
        },
        config);

    std::thread worker([&] { server.serve_connection(listener.accept()); });
    auto client = vhttp::net::TcpStream::connect("127.0.0.1", port);
    client.set_receive_timeout(std::chrono::milliseconds(3000));

    const auto started = std::chrono::steady_clock::now();
    const auto wire = receive_until_close(client);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    worker.join();

    expect(wire.empty(), "idle socket should close without an HTTP response");
    expect(elapsed < std::chrono::seconds(2), "idle timeout should retire the connection promptly");
}

}  // namespace

int main() {
    vhttp::net::SocketRuntime runtime;
    serves_two_pipelined_requests_on_one_connection();
    chunked_request_can_be_followed_by_pipelined_request();
    max_request_limit_forces_close();
    idle_timeout_retires_silent_connection();
    std::cout << "connection integration tests passed\n";
}
