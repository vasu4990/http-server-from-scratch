#include "vhttp/net/socket.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace vhttp::net {
namespace {

void close_native(NativeSocket socket) noexcept {
    if (socket == invalid_socket) {
        return;
    }
#ifdef _WIN32
    ::closesocket(socket);
#else
    ::close(socket);
#endif
}

[[noreturn]] void throw_socket_error(const char* operation) {
#ifdef _WIN32
    throw std::system_error(WSAGetLastError(), std::system_category(), operation);
#else
    throw std::system_error(errno, std::generic_category(), operation);
#endif
}

}  // namespace

SocketRuntime::SocketRuntime() {
#ifdef _WIN32
    WSADATA data{};
    const int rc = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (rc != 0) {
        throw std::system_error(rc, std::system_category(), "WSAStartup");
    }
#endif
}

SocketRuntime::~SocketRuntime() {
#ifdef _WIN32
    ::WSACleanup();
#endif
}

TcpStream::~TcpStream() { close(); }

TcpStream::TcpStream(TcpStream&& other) noexcept : socket_(std::exchange(other.socket_, invalid_socket)) {}

TcpStream& TcpStream::operator=(TcpStream&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = std::exchange(other.socket_, invalid_socket);
    }
    return *this;
}

std::ptrdiff_t TcpStream::receive(char* buffer, std::size_t size) {
#ifdef _WIN32
    const int capped = size > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(size);
    const int rc = ::recv(socket_, buffer, capped, 0);
    if (rc == SOCKET_ERROR) {
        throw_socket_error("recv");
    }
    return static_cast<std::ptrdiff_t>(rc);
#else
    const auto rc = ::recv(socket_, buffer, size, 0);
    if (rc < 0) {
        if (errno == EINTR) {
            return receive(buffer, size);
        }
        throw_socket_error("recv");
    }
    return rc;
#endif
}

void TcpStream::send_all(std::string_view bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
#ifdef _WIN32
        const auto remaining = bytes.size() - sent;
        const int capped = remaining > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(remaining);
        const int rc = ::send(socket_, bytes.data() + sent, capped, 0);
        if (rc == SOCKET_ERROR) {
            throw_socket_error("send");
        }
#else
        const auto rc = ::send(socket_, bytes.data() + sent, bytes.size() - sent, 0);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw_socket_error("send");
        }
#endif
        if (rc == 0) {
            throw std::runtime_error("send returned zero before payload was transmitted");
        }
        sent += static_cast<std::size_t>(rc);
    }
}

void TcpStream::close() noexcept {
    close_native(socket_);
    socket_ = invalid_socket;
}

TcpListener::~TcpListener() { close(); }

TcpListener::TcpListener(TcpListener&& other) noexcept : socket_(std::exchange(other.socket_, invalid_socket)) {}

TcpListener& TcpListener::operator=(TcpListener&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = std::exchange(other.socket_, invalid_socket);
    }
    return *this;
}

TcpListener TcpListener::bind(std::string_view host, std::uint16_t port, int backlog) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    const std::string host_string(host);
    const std::string port_string = std::to_string(port);
    addrinfo* results = nullptr;
    const int gai = ::getaddrinfo(host.empty() ? nullptr : host_string.c_str(), port_string.c_str(), &hints, &results);
    if (gai != 0) {
#ifdef _WIN32
        throw std::runtime_error("getaddrinfo failed with code " + std::to_string(gai));
#else
        throw std::runtime_error(std::string("getaddrinfo failed: ") + ::gai_strerror(gai));
#endif
    }

    NativeSocket bound = invalid_socket;
    for (auto* current = results; current != nullptr; current = current->ai_next) {
        NativeSocket candidate = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (candidate == invalid_socket) {
            continue;
        }

        int reuse = 1;
#ifdef _WIN32
        ::setsockopt(candidate, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
        ::setsockopt(candidate, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

        if (::bind(candidate, current->ai_addr, static_cast<int>(current->ai_addrlen)) == 0 &&
            ::listen(candidate, backlog) == 0) {
            bound = candidate;
            break;
        }
        close_native(candidate);
    }

    ::freeaddrinfo(results);
    if (bound == invalid_socket) {
        throw_socket_error("bind/listen");
    }
    return TcpListener(bound);
}

TcpStream TcpListener::accept() {
    const NativeSocket client = ::accept(socket_, nullptr, nullptr);
    if (client == invalid_socket) {
        throw_socket_error("accept");
    }
    return TcpStream(client);
}

void TcpListener::close() noexcept {
    close_native(socket_);
    socket_ = invalid_socket;
}

}  // namespace vhttp::net
