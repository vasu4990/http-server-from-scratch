#pragma once

#include "vhttp/http/request.hpp"
#include "vhttp/http/response.hpp"
#include "vhttp/net/socket.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace vhttp::server {

using Handler = std::function<http::HttpResponse(const http::HttpRequest&)>;

class Server {
public:
    explicit Server(Handler handler);

    void listen_and_serve(std::string host, std::uint16_t port);

private:
    void handle_connection(net::TcpStream stream);

    Handler handler_;
};

}  // namespace vhttp::server
