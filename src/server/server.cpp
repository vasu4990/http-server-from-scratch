#include "vhttp/server/server.hpp"

#include "vhttp/http/parser.hpp"
#include "vhttp/net/socket.hpp"
#include "vhttp/server/connection.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace vhttp::server {

Server::Server(Handler handler, ConnectionConfig config)
    : handler_(std::move(handler)), config_(config) {
    if (!handler_) {
        throw std::invalid_argument("Server requires a request handler");
    }
    if (config_.max_requests_per_connection == 0) {
        throw std::invalid_argument("max_requests_per_connection must be greater than zero");
    }
    if (config_.idle_timeout.count() <= 0) {
        throw std::invalid_argument("idle_timeout must be positive");
    }
}

void Server::listen_and_serve(std::string host, std::uint16_t port) {
    net::SocketRuntime runtime;
    auto listener = net::TcpListener::bind(host, port);

    std::cout << "vhttp listening on " << (host.empty() ? "0.0.0.0" : host) << ':' << port << '\n';

    while (true) {
        try {
            handle_connection(listener.accept());
        } catch (const std::exception& ex) {
            std::cerr << "connection error: " << ex.what() << '\n';
        }
    }
}

void Server::handle_connection(net::TcpStream stream) {
    stream.set_receive_timeout(config_.idle_timeout);

    http::HttpRequestParser parser;
    std::array<char, 8192> buffer{};
    auto status = http::ParseStatus::need_more_data;
    std::size_t requests_served = 0;

    while (true) {
        while (status == http::ParseStatus::need_more_data) {
            std::ptrdiff_t received = 0;
            try {
                received = stream.receive(buffer.data(), buffer.size());
            } catch (const net::SocketTimeoutError&) {
                // An idle persistent connection is simply retired. No response is
                // written because a complete HTTP request was not available.
                return;
            }

            if (received == 0) {
                return;
            }

            status = parser.feed(
                std::string_view(buffer.data(), static_cast<std::size_t>(received)));
        }

        if (status == http::ParseStatus::error) {
            auto response = http::HttpResponse::text(400, "Bad Request", "Bad Request\n");
            stream.send_all(response.serialize(false));
            return;
        }

        const auto& request = parser.request();
        bool keep_alive = request_allows_persistent_connection(request);
        auto response = handler_(request);

        if (response_requests_connection_close(response)) {
            keep_alive = false;
        }

        ++requests_served;
        if (requests_served >= config_.max_requests_per_connection) {
            keep_alive = false;
        }

        stream.send_all(response.serialize(keep_alive, request.method == "HEAD"));
        if (!keep_alive) {
            return;
        }

        // TCP reads are not HTTP message boundaries. The parser may already hold
        // bytes for the next pipelined request. Move them out before reset, then
        // immediately feed them into the fresh parser state instead of reading
        // from the socket and accidentally stalling.
        std::string remaining = parser.take_remaining();
        parser.reset();
        status = remaining.empty() ? http::ParseStatus::need_more_data : parser.feed(remaining);
    }
}

}  // namespace vhttp::server
