#include "vhttp/router/router.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

vhttp::http::HttpRequest request(std::string method, std::string target) {
    vhttp::http::HttpRequest req;
    req.method = std::move(method);
    req.target = std::move(target);
    req.version = "HTTP/1.1";
    return req;
}

void matches_static_routes_before_parameters() {
    vhttp::router::Router router;
    router.get("/users/:id", [](const auto& req) {
        return vhttp::http::HttpResponse::text(200, "OK", "param=" + std::string(req.path_param("id")));
    });
    router.get("/users/me", [](const auto&) {
        return vhttp::http::HttpResponse::text(200, "OK", "static");
    });

    expect(router.dispatch(request("GET", "/users/me")).body == "static", "static segment wins over parameter");
    expect(router.dispatch(request("GET", "/users/42")).body == "param=42", "parameter route extracts value");
}

void parses_query_parameters_and_repeated_keys() {
    vhttp::router::Router router;
    router.get("/search", [](const auto& req) {
        expect(req.query("q") == "robotics", "first query value available");
        expect(req.query_params.at("tag").size() == 2, "repeated query keys preserved");
        return vhttp::http::HttpResponse::text(200, "OK", "ok");
    });

    expect(router.dispatch(request("GET", "/search?q=robotics&tag=ros2&tag=slam")).status == 200,
           "query route dispatched");
}

void returns_404_and_405_with_allow() {
    vhttp::router::Router router;
    router.get("/items", [](const auto&) { return vhttp::http::HttpResponse::text(200, "OK", "items"); });
    router.post("/items", [](const auto&) { return vhttp::http::HttpResponse::text(201, "Created", "created"); });

    expect(router.dispatch(request("GET", "/missing")).status == 404, "missing path returns 404");

    const auto response = router.dispatch(request("DELETE", "/items"));
    expect(response.status == 405, "known path with wrong method returns 405");
    expect(response.headers.at("Allow") == "GET, HEAD, OPTIONS, POST", "Allow header is deterministic");
}

void supports_head_fallback_and_automatic_options() {
    vhttp::router::Router router;
    router.get("/health", [](const auto&) { return vhttp::http::HttpResponse::text(200, "OK", "healthy"); });

    const auto head = router.dispatch(request("HEAD", "/health"));
    expect(head.status == 200 && head.body == "healthy", "HEAD resolves through GET handler before wire suppression");

    const auto options = router.dispatch(request("OPTIONS", "/health"));
    expect(options.status == 204, "OPTIONS auto response uses 204");
    expect(options.headers.at("Allow") == "GET, HEAD, OPTIONS", "OPTIONS exposes allowed methods");
}

void rejects_ambiguous_or_duplicate_routes() {
    vhttp::router::Router router;
    router.get("/users/:id", [](const auto&) { return vhttp::http::HttpResponse{}; });

    bool duplicate = false;
    try {
        router.get("/users/:id", [](const auto&) { return vhttp::http::HttpResponse{}; });
    } catch (const std::invalid_argument&) {
        duplicate = true;
    }
    expect(duplicate, "duplicate method and route rejected");

    bool ambiguous = false;
    try {
        router.post("/users/:name", [](const auto&) { return vhttp::http::HttpResponse{}; });
    } catch (const std::invalid_argument&) {
        ambiguous = true;
    }
    expect(ambiguous, "different parameter names at same route depth rejected");
}

}  // namespace

int main() {
    matches_static_routes_before_parameters();
    parses_query_parameters_and_repeated_keys();
    returns_404_and_405_with_allow();
    supports_head_fallback_and_automatic_options();
    rejects_ambiguous_or_duplicate_routes();
    std::cout << "router tests passed\n";
}
