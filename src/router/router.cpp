#include "vhttp/router/router.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace vhttp::router {
namespace {

std::string uppercase_ascii(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char ch : value) {
        out.push_back(static_cast<char>(std::toupper(ch)));
    }
    return out;
}

}  // namespace

ParsedTarget parse_target(std::string_view target) {
    ParsedTarget parsed;

    const auto question = target.find('?');
    const auto path_view = target.substr(0, question);
    parsed.path.assign(path_view.empty() ? std::string_view{"/"} : path_view);

    if (question == std::string_view::npos) {
        return parsed;
    }

    std::string_view query = target.substr(question + 1);
    while (!query.empty()) {
        const auto amp = query.find('&');
        const auto pair = query.substr(0, amp);

        if (!pair.empty()) {
            const auto equals = pair.find('=');
            const auto key = pair.substr(0, equals);
            const auto value = equals == std::string_view::npos ? std::string_view{} : pair.substr(equals + 1);
            if (!key.empty()) {
                parsed.query[std::string(key)].emplace_back(value);
            }
        }

        if (amp == std::string_view::npos) {
            break;
        }
        query.remove_prefix(amp + 1);
    }

    return parsed;
}

struct Router::Node {
    std::unordered_map<std::string, std::unique_ptr<Node>> static_children;
    std::unique_ptr<Node> parameter_child;
    std::string parameter_name;
    std::unordered_map<std::string, Handler> handlers;
};

struct Router::Match {
    const Node* node = nullptr;
    std::unordered_map<std::string, std::string> params;
};

Router::Router() : root_(std::make_unique<Node>()) {}
Router::~Router() = default;
Router::Router(Router&&) noexcept = default;
Router& Router::operator=(Router&&) noexcept = default;

std::vector<std::string_view> Router::split_path(std::string_view path) {
    std::vector<std::string_view> segments;
    if (path.empty() || path == "/") {
        return segments;
    }

    if (path.front() == '/') {
        path.remove_prefix(1);
    }

    while (!path.empty()) {
        const auto slash = path.find('/');
        const auto segment = path.substr(0, slash);
        if (!segment.empty()) {
            segments.push_back(segment);
        }
        if (slash == std::string_view::npos) {
            break;
        }
        path.remove_prefix(slash + 1);
    }
    return segments;
}

std::string Router::normalize_method(std::string_view method) {
    if (method.empty()) {
        throw std::invalid_argument("route method must not be empty");
    }
    return uppercase_ascii(method);
}

Router& Router::add(std::string method, std::string pattern, Handler handler) {
    if (!handler) {
        throw std::invalid_argument("route handler must be callable");
    }
    if (pattern.empty() || pattern.front() != '/' || pattern.find('?') != std::string::npos) {
        throw std::invalid_argument("route pattern must be an absolute path without a query string");
    }

    Node* node = root_.get();
    std::unordered_set<std::string> seen_params;

    for (const auto segment : split_path(pattern)) {
        if (!segment.empty() && segment.front() == ':') {
            const auto name = segment.substr(1);
            if (name.empty()) {
                throw std::invalid_argument("route parameter name must not be empty");
            }
            if (!seen_params.emplace(name).second) {
                throw std::invalid_argument("route parameter names must be unique within a pattern");
            }

            if (!node->parameter_child) {
                node->parameter_child = std::make_unique<Node>();
                node->parameter_name.assign(name);
            } else if (node->parameter_name != name) {
                throw std::invalid_argument("ambiguous parameter name at the same route depth");
            }
            node = node->parameter_child.get();
        } else {
            auto& child = node->static_children[std::string(segment)];
            if (!child) {
                child = std::make_unique<Node>();
            }
            node = child.get();
        }
    }

    const auto normalized = normalize_method(method);
    if (node->handlers.contains(normalized)) {
        throw std::invalid_argument("duplicate route registration for method and pattern");
    }
    node->handlers.emplace(normalized, std::move(handler));
    return *this;
}

Router& Router::get(std::string pattern, Handler handler) { return add("GET", std::move(pattern), std::move(handler)); }
Router& Router::head(std::string pattern, Handler handler) { return add("HEAD", std::move(pattern), std::move(handler)); }
Router& Router::post(std::string pattern, Handler handler) { return add("POST", std::move(pattern), std::move(handler)); }
Router& Router::put(std::string pattern, Handler handler) { return add("PUT", std::move(pattern), std::move(handler)); }
Router& Router::patch(std::string pattern, Handler handler) { return add("PATCH", std::move(pattern), std::move(handler)); }
Router& Router::del(std::string pattern, Handler handler) { return add("DELETE", std::move(pattern), std::move(handler)); }
Router& Router::options(std::string pattern, Handler handler) { return add("OPTIONS", std::move(pattern), std::move(handler)); }

Router::Match Router::match_path(std::string_view path) const {
    Match match;
    const Node* node = root_.get();

    for (const auto segment : split_path(path)) {
        const auto static_it = node->static_children.find(std::string(segment));
        if (static_it != node->static_children.end()) {
            node = static_it->second.get();
            continue;
        }

        if (node->parameter_child) {
            match.params[node->parameter_name] = std::string(segment);
            node = node->parameter_child.get();
            continue;
        }

        return {};
    }

    match.node = node;
    return match;
}

std::string Router::allow_header(const Node& node) {
    std::vector<std::string> methods;
    methods.reserve(node.handlers.size() + 2);
    for (const auto& [method, _] : node.handlers) {
        methods.push_back(method);
    }
    if (node.handlers.contains("GET") && !node.handlers.contains("HEAD")) {
        methods.emplace_back("HEAD");
    }
    if (!node.handlers.contains("OPTIONS")) {
        methods.emplace_back("OPTIONS");
    }

    std::sort(methods.begin(), methods.end());
    methods.erase(std::unique(methods.begin(), methods.end()), methods.end());

    std::string out;
    for (std::size_t i = 0; i < methods.size(); ++i) {
        if (i != 0) {
            out.append(", ");
        }
        out.append(methods[i]);
    }
    return out;
}

http::HttpResponse Router::dispatch(const http::HttpRequest& request) const {
    const auto parsed = parse_target(request.target);
    auto match = match_path(parsed.path);
    if (!match.node || match.node->handlers.empty()) {
        return http::HttpResponse::text(404, "Not Found", "Not Found\n");
    }

    http::HttpRequest routed = request;
    routed.path = parsed.path;
    routed.path_params = std::move(match.params);
    routed.query_params = parsed.query;

    const auto method = normalize_method(request.method);
    if (method == "OPTIONS" && !match.node->handlers.contains("OPTIONS")) {
        http::HttpResponse response;
        response.set_status(204, "No Content");
        response.set_header("Allow", allow_header(*match.node));
        return response;
    }

    auto handler_it = match.node->handlers.find(method);
    if (handler_it == match.node->handlers.end() && method == "HEAD") {
        handler_it = match.node->handlers.find("GET");
    }

    if (handler_it == match.node->handlers.end()) {
        auto response = http::HttpResponse::text(405, "Method Not Allowed", "Method Not Allowed\n");
        response.set_header("Allow", allow_header(*match.node));
        return response;
    }

    return handler_it->second(routed);
}

}  // namespace vhttp::router
