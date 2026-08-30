#pragma once

#include "vhttp/http/request.hpp"
#include "vhttp/http/response.hpp"
#include "vhttp/net/socket.hpp"
#include "vhttp/server/connection.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace vhttp::server {

using Handler = std::function<http::HttpResponse(const http::HttpRequest&)>;

class Server {
public:
    explicit Server(Handler handler, ConnectionConfig config = {});

    void listen_and_serve(std::string host, std::uint16_t port);

    // Serve one already-accepted TCP connection until HTTP policy closes it.
    // This is useful for embedders and enables deterministic loopback tests
    // without exposing the listener's infinite accept loop.
    void serve_connection(net::TcpStream stream);

private:
    Handler handler_;
    ConnectionConfig config_;
};

}  // namespace vhttp::server
