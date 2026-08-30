#include "vhttp/server/server.hpp"

#include "vhttp/http/parser.hpp"
#include "vhttp/net/socket.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <utility>

namespace vhttp::server {

Server::Server(Handler handler) : handler_(std::move(handler)) {
    if (!handler_) {
        throw std::invalid_argument("Server requires a request handler");
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
    http::HttpRequestParser parser;
    std::array<char, 8192> buffer{};

    while (true) {
        const auto received = stream.receive(buffer.data(), buffer.size());
        if (received == 0) {
            return;
        }

        const auto status = parser.feed(std::string_view(buffer.data(), static_cast<std::size_t>(received)));
        if (status == http::ParseStatus::need_more_data) {
            continue;
        }

        if (status == http::ParseStatus::error) {
            auto response = http::HttpResponse::text(400, "Bad Request", "Bad Request\n");
            stream.send_all(response.serialize(false));
            return;
        }

        auto response = handler_(parser.request());
        stream.send_all(response.serialize(false));
        return;
    }
}

}  // namespace vhttp::server
