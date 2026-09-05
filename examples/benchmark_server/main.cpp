#include "vhttp/router/router.hpp"
#include "vhttp/server/thread_pool_runtime.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

std::uint64_t parse_positive(std::string_view text, std::string_view name) {
    std::size_t consumed = 0;
    const auto value = std::stoull(std::string(text), &consumed, 10);
    if (consumed != text.size() || value == 0) {
        throw std::invalid_argument(std::string(name) + " must be a positive integer");
    }
    return value;
}

std::uint16_t parse_port(std::string_view text) {
    const auto value = parse_positive(text, "port");
    if (value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::out_of_range("port must be between 1 and 65535");
    }
    return static_cast<std::uint16_t>(value);
}

std::size_t parse_size(std::string_view text, std::string_view name) {
    const auto value = parse_positive(text, name);
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::out_of_range(std::string(name) + " is too large for this platform");
    }
    return static_cast<std::size_t>(value);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::uint16_t port = argc > 1 ? parse_port(argv[1]) : 8081;
        const std::size_t workers = argc > 2 ? parse_size(argv[2], "workers") : 4;
        const std::size_t pending = argc > 3 ? parse_size(argv[3], "pending queue") : 256;
        const auto duration_seconds = argc > 4 ? parse_positive(argv[4], "duration seconds") : 30;
        const std::size_t payload_bytes = argc > 5 ? parse_size(argv[5], "payload bytes") : 128;

        constexpr std::size_t max_benchmark_payload = 1024U * 1024U;
        if (payload_bytes > max_benchmark_payload) {
            throw std::out_of_range("payload bytes must be <= 1048576");
        }

        const std::string payload(payload_bytes, 'x');

        vhttp::router::Router router;
        router.get("/health", [](const auto&) {
            return vhttp::http::HttpResponse::text(200, "OK", "healthy\n");
        });
        router.get("/bench", [&payload](const auto&) {
            return vhttp::http::HttpResponse::text(200, "OK", payload);
        });

        vhttp::server::ThreadPoolConfig pool_config;
        pool_config.worker_count = workers;
        pool_config.max_pending_connections = pending;

        vhttp::server::ThreadPoolRuntime runtime(
            [&router](const auto& request) { return router.dispatch(request); },
            {},
            pool_config);

        std::exception_ptr server_failure;
        std::thread server_thread([&] {
            try {
                runtime.run("127.0.0.1", port);
            } catch (...) {
                server_failure = std::current_exception();
            }
        });

        if (!runtime.wait_until_listening(std::chrono::seconds(5))) {
            runtime.request_stop();
            if (server_thread.joinable()) {
                server_thread.join();
            }
            if (server_failure) {
                std::rethrow_exception(server_failure);
            }
            throw std::runtime_error("benchmark server did not begin listening within 5 seconds");
        }

        std::cout << "vhttp benchmark server\n"
                  << "  endpoint: http://127.0.0.1:" << runtime.bound_port() << "/bench\n"
                  << "  workers: " << workers << '\n'
                  << "  pending queue: " << pending << '\n'
                  << "  payload bytes: " << payload_bytes << '\n'
                  << "  duration seconds: " << duration_seconds << '\n'
                  << std::flush;

        std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
        runtime.request_stop();
        server_thread.join();

        if (server_failure) {
            std::rethrow_exception(server_failure);
        }

        const auto stats = runtime.stats();
        std::cout << "final runtime stats\n"
                  << "  accepted: " << stats.accepted << '\n'
                  << "  rejected: " << stats.rejected << '\n'
                  << "  completed: " << stats.completed << '\n'
                  << "  failed: " << stats.failed << '\n'
                  << "  active: " << stats.active << '\n'
                  << "  queued: " << stats.queued << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << '\n';
        std::cerr << "usage: vhttp_bench_server [port] [workers] [pending_queue] [duration_seconds] [payload_bytes]\n";
        return 1;
    }
}
