#pragma once

#include "vhttp/http/request.hpp"
#include "vhttp/http/response.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace vhttp::static_files {

struct StaticFileConfig {
    std::filesystem::path document_root;
    std::string url_prefix = "/static";
    std::string index_file = "index.html";
    std::uintmax_t max_file_bytes = 64ULL * 1024ULL * 1024ULL;
    bool serve_index = true;
};

struct ByteRange {
    std::uintmax_t first = 0;
    std::uintmax_t last = 0;
};

[[nodiscard]] std::string mime_type_for_path(const std::filesystem::path& path);
[[nodiscard]] std::optional<ByteRange> parse_single_byte_range(std::string_view value,
                                                               std::uintmax_t size);

class StaticFileHandler {
public:
    explicit StaticFileHandler(StaticFileConfig config);

    // Returns nullopt when the request target is outside this handler's URL
    // prefix. Once the prefix matches, all outcomes are represented as an HTTP
    // response so callers can safely compose this handler with a router.
    [[nodiscard]] std::optional<http::HttpResponse> try_serve(const http::HttpRequest& request) const;

    [[nodiscard]] const std::filesystem::path& document_root() const noexcept { return root_; }
    [[nodiscard]] std::string_view url_prefix() const noexcept { return config_.url_prefix; }

private:
    StaticFileConfig config_;
    std::filesystem::path root_;
};

}  // namespace vhttp::static_files
