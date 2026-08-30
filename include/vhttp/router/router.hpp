#pragma once

#include "vhttp/http/request.hpp"
#include "vhttp/http/response.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vhttp::router {

using Handler = std::function<http::HttpResponse(const http::HttpRequest&)>;

struct ParsedTarget {
    std::string path;
    std::unordered_map<std::string, std::vector<std::string>> query;
};

[[nodiscard]] ParsedTarget parse_target(std::string_view target);

class Router {
public:
    Router();
    ~Router();
    Router(Router&&) noexcept;
    Router& operator=(Router&&) noexcept;
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    Router& add(std::string method, std::string pattern, Handler handler);
    Router& get(std::string pattern, Handler handler);
    Router& head(std::string pattern, Handler handler);
    Router& post(std::string pattern, Handler handler);
    Router& put(std::string pattern, Handler handler);
    Router& patch(std::string pattern, Handler handler);
    Router& del(std::string pattern, Handler handler);
    Router& options(std::string pattern, Handler handler);

    [[nodiscard]] http::HttpResponse dispatch(const http::HttpRequest& request) const;

private:
    struct Node;
    struct Match;

    std::unique_ptr<Node> root_;

    [[nodiscard]] Match match_path(std::string_view path) const;
    [[nodiscard]] static std::vector<std::string_view> split_path(std::string_view path);
    [[nodiscard]] static std::string normalize_method(std::string_view method);
    [[nodiscard]] static std::string allow_header(const Node& node);
};

}  // namespace vhttp::router
