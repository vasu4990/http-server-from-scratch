#include "vhttp/static_files/static_file_handler.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace vhttp::static_files {
namespace {

std::string ascii_lower(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char ch : value) {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

bool ascii_iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const auto a = static_cast<char>(std::tolower(static_cast<unsigned char>(lhs[i])));
        const auto b = static_cast<char>(std::tolower(static_cast<unsigned char>(rhs[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

std::string_view trim_ows(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

int hex_digit(unsigned char ch) {
    if (ch >= '0' && ch <= '9') {
        return static_cast<int>(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + static_cast<int>(ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + static_cast<int>(ch - 'A');
    }
    return -1;
}

std::optional<std::string> decode_segment(std::string_view segment) {
    std::string decoded;
    decoded.reserve(segment.size());

    for (std::size_t i = 0; i < segment.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(segment[i]);
        if (ch == '%') {
            if (i + 2 >= segment.size()) {
                return std::nullopt;
            }
            const int hi = hex_digit(static_cast<unsigned char>(segment[i + 1]));
            const int lo = hex_digit(static_cast<unsigned char>(segment[i + 2]));
            if (hi < 0 || lo < 0) {
                return std::nullopt;
            }
            ch = static_cast<unsigned char>((hi << 4) | lo);
            i += 2;
        }

        // Encoded separators, Windows separators, NUL, and controls are rejected
        // before the decoded path ever reaches std::filesystem.
        if (ch == '/' || ch == '\\' || ch == 0 || ch < 0x20U || ch == 0x7FU) {
            return std::nullopt;
        }
        decoded.push_back(static_cast<char>(ch));
    }

    return decoded;
}

bool path_is_within(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    if (candidate == root) {
        return true;
    }
    const auto relative = candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    for (const auto& component : relative) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

std::time_t file_time_to_time_t(std::filesystem::file_time_type value) {
    const auto system_value = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        value - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(system_value);
}

std::string format_http_date(std::time_t value) {
    std::tm tm{};
#ifdef _WIN32
    if (gmtime_s(&tm, &value) != 0) {
        return {};
    }
#else
    if (gmtime_r(&value, &tm) == nullptr) {
        return {};
    }
#endif
    char buffer[64]{};
    if (std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &tm) == 0) {
        return {};
    }
    return buffer;
}

std::optional<std::time_t> parse_http_date(std::string_view value) {
    std::tm tm{};
    std::istringstream in{std::string(value)};
    in.imbue(std::locale::classic());
    in >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
    if (in.fail()) {
        return std::nullopt;
    }
    in >> std::ws;
    if (!in.eof()) {
        return std::nullopt;
    }
#ifdef _WIN32
    const auto converted = _mkgmtime(&tm);
#else
    const auto converted = timegm(&tm);
#endif
    if (converted == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    return converted;
}

std::string make_weak_etag(std::uintmax_t size, std::filesystem::file_time_type modified) {
    std::ostringstream out;
    out << "W/\"" << std::hex << size << '-'
        << static_cast<long long>(modified.time_since_epoch().count()) << "\"";
    return out.str();
}

std::string_view strip_weak_prefix(std::string_view tag) {
    tag = trim_ows(tag);
    if (tag.size() >= 2 && (tag[0] == 'W' || tag[0] == 'w') && tag[1] == '/') {
        tag.remove_prefix(2);
        tag = trim_ows(tag);
    }
    return tag;
}

bool if_none_match_matches(std::string_view value, std::string_view current_etag) {
    while (!value.empty()) {
        const auto comma = value.find(',');
        const auto token = trim_ows(value.substr(0, comma));
        if (token == "*" || strip_weak_prefix(token) == strip_weak_prefix(current_etag)) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        value.remove_prefix(comma + 1);
    }
    return false;
}

bool parse_uintmax(std::string_view text, std::uintmax_t& out) {
    if (text.empty()) {
        return false;
    }
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

std::optional<std::string> read_file_slice(const std::filesystem::path& path,
                                           std::uintmax_t first,
                                           std::uintmax_t length) {
    if (length > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
        first > static_cast<std::uintmax_t>(std::numeric_limits<std::streamoff>::max()) ||
        length > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    input.seekg(static_cast<std::streamoff>(first), std::ios::beg);
    if (!input) {
        return std::nullopt;
    }

    std::string body(static_cast<std::size_t>(length), '\0');
    if (length != 0) {
        input.read(body.data(), static_cast<std::streamsize>(length));
        if (input.gcount() != static_cast<std::streamsize>(length)) {
            return std::nullopt;
        }
    }
    return body;
}

http::HttpResponse not_found() {
    return http::HttpResponse::text(404, "Not Found", "Not Found\n");
}

http::HttpResponse bad_request() {
    return http::HttpResponse::text(400, "Bad Request", "Bad Request\n");
}

void add_static_headers(http::HttpResponse& response,
                        std::string_view etag,
                        std::string_view last_modified) {
    response.set_header("Accept-Ranges", "bytes");
    response.set_header("ETag", std::string(etag));
    if (!last_modified.empty()) {
        response.set_header("Last-Modified", std::string(last_modified));
    }
    response.set_header("X-Content-Type-Options", "nosniff");
}

}  // namespace

std::string mime_type_for_path(const std::filesystem::path& path) {
    const auto extension = ascii_lower(path.extension().string());
    if (extension == ".html" || extension == ".htm") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js" || extension == ".mjs") return "text/javascript; charset=utf-8";
    if (extension == ".json") return "application/json; charset=utf-8";
    if (extension == ".txt" || extension == ".log") return "text/plain; charset=utf-8";
    if (extension == ".xml") return "application/xml; charset=utf-8";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".webp") return "image/webp";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".pdf") return "application/pdf";
    if (extension == ".wasm") return "application/wasm";
    if (extension == ".mp4") return "video/mp4";
    if (extension == ".webm") return "video/webm";
    if (extension == ".woff") return "font/woff";
    if (extension == ".woff2") return "font/woff2";
    return "application/octet-stream";
}

std::optional<ByteRange> parse_single_byte_range(std::string_view value, std::uintmax_t size) {
    value = trim_ows(value);
    if (size == 0 || value.size() < 6 || !ascii_iequals(value.substr(0, 6), "bytes=")) {
        return std::nullopt;
    }

    auto spec = trim_ows(value.substr(6));
    if (spec.empty() || spec.find(',') != std::string_view::npos) {
        return std::nullopt;
    }

    const auto dash = spec.find('-');
    if (dash == std::string_view::npos || spec.find('-', dash + 1) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto first_text = spec.substr(0, dash);
    const auto last_text = spec.substr(dash + 1);

    if (first_text.empty()) {
        std::uintmax_t suffix = 0;
        if (!parse_uintmax(last_text, suffix) || suffix == 0) {
            return std::nullopt;
        }
        const auto take = std::min(suffix, size);
        return ByteRange{size - take, size - 1};
    }

    std::uintmax_t first = 0;
    if (!parse_uintmax(first_text, first) || first >= size) {
        return std::nullopt;
    }

    std::uintmax_t last = size - 1;
    if (!last_text.empty()) {
        if (!parse_uintmax(last_text, last) || last < first) {
            return std::nullopt;
        }
        last = std::min(last, size - 1);
    }
    return ByteRange{first, last};
}

StaticFileHandler::StaticFileHandler(StaticFileConfig config) : config_(std::move(config)) {
    if (config_.document_root.empty()) {
        throw std::invalid_argument("static document_root must not be empty");
    }
    if (config_.url_prefix.empty() || config_.url_prefix.front() != '/' ||
        config_.url_prefix.find('?') != std::string::npos) {
        throw std::invalid_argument("static url_prefix must be an absolute path without a query");
    }
    while (config_.url_prefix.size() > 1 && config_.url_prefix.back() == '/') {
        config_.url_prefix.pop_back();
    }
    if (config_.max_file_bytes == 0) {
        throw std::invalid_argument("static max_file_bytes must be greater than zero");
    }
    if (config_.serve_index &&
        (config_.index_file.empty() || config_.index_file == "." || config_.index_file == ".." ||
         config_.index_file.find('/') != std::string::npos ||
         config_.index_file.find('\\') != std::string::npos)) {
        throw std::invalid_argument("static index_file must be one safe filename");
    }

    std::error_code ec;
    root_ = std::filesystem::weakly_canonical(config_.document_root, ec);
    if (ec || !std::filesystem::is_directory(root_, ec) || ec) {
        throw std::invalid_argument("static document_root must resolve to an existing directory");
    }
}

std::optional<http::HttpResponse> StaticFileHandler::try_serve(const http::HttpRequest& request) const {
    const std::string_view target(request.target);
    const auto question = target.find('?');
    const auto path = target.substr(0, question);

    std::string_view relative_text;
    if (config_.url_prefix == "/") {
        if (path.empty() || path.front() != '/') {
            return std::nullopt;
        }
        relative_text = path.substr(1);
    } else if (path == config_.url_prefix) {
        relative_text = {};
    } else if (path.size() > config_.url_prefix.size() &&
               path.compare(0, config_.url_prefix.size(), config_.url_prefix) == 0 &&
               path[config_.url_prefix.size()] == '/') {
        relative_text = path.substr(config_.url_prefix.size() + 1);
    } else {
        return std::nullopt;
    }

    if (request.method != "GET" && request.method != "HEAD") {
        auto response = http::HttpResponse::text(405, "Method Not Allowed", "Method Not Allowed\n");
        response.set_header("Allow", "GET, HEAD");
        return response;
    }

    std::filesystem::path relative;
    while (!relative_text.empty()) {
        const auto slash = relative_text.find('/');
        const auto raw_segment = relative_text.substr(0, slash);
        if (!raw_segment.empty()) {
            const auto decoded = decode_segment(raw_segment);
            if (!decoded) {
                return bad_request();
            }
            if (*decoded == "." || *decoded == "..") {
                return not_found();
            }
            relative /= std::filesystem::path(*decoded);
        }
        if (slash == std::string_view::npos) {
            break;
        }
        relative_text.remove_prefix(slash + 1);
    }

    std::error_code ec;
    auto candidate = std::filesystem::weakly_canonical(root_ / relative, ec);
    if (ec || !path_is_within(root_, candidate)) {
        return not_found();
    }

    if (std::filesystem::is_directory(candidate, ec) && !ec) {
        if (!config_.serve_index) {
            return not_found();
        }
        candidate = std::filesystem::weakly_canonical(candidate / config_.index_file, ec);
        if (ec || !path_is_within(root_, candidate)) {
            return not_found();
        }
    }

    if (!std::filesystem::is_regular_file(candidate, ec) || ec) {
        return not_found();
    }

    const auto size = std::filesystem::file_size(candidate, ec);
    if (ec || size > config_.max_file_bytes ||
        size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        return not_found();
    }

    const auto modified = std::filesystem::last_write_time(candidate, ec);
    if (ec) {
        return not_found();
    }
    const auto modified_time = file_time_to_time_t(modified);
    const auto last_modified = format_http_date(modified_time);
    const auto etag = make_weak_etag(size, modified);

    const auto if_none_match = request.header("if-none-match");
    if (!if_none_match.empty()) {
        if (if_none_match_matches(if_none_match, etag)) {
            http::HttpResponse response;
            response.set_status(304, "Not Modified");
            add_static_headers(response, etag, last_modified);
            return response;
        }
    } else if (const auto ims = request.header("if-modified-since"); !ims.empty()) {
        if (const auto parsed = parse_http_date(trim_ows(ims)); parsed && modified_time <= *parsed) {
            http::HttpResponse response;
            response.set_status(304, "Not Modified");
            add_static_headers(response, etag, last_modified);
            return response;
        }
    }

    bool use_range = false;
    std::optional<ByteRange> range;
    if (const auto range_header = request.header("range"); !range_header.empty()) {
        bool if_range_allows = true;
        if (const auto if_range = trim_ows(request.header("if-range")); !if_range.empty()) {
            // The generated ETag is weak, so it is never accepted as an If-Range
            // validator. A valid date can authorize the range when the resource
            // has not changed since that instant.
            if (if_range.front() == '"' ||
                (if_range.size() >= 2 && (if_range[0] == 'W' || if_range[0] == 'w') && if_range[1] == '/')) {
                if_range_allows = false;
            } else {
                const auto parsed = parse_http_date(if_range);
                if_range_allows = parsed.has_value() && modified_time <= *parsed;
            }
        }

        if (if_range_allows) {
            range = parse_single_byte_range(range_header, size);
            if (!range) {
                auto response = http::HttpResponse::text(
                    416, "Range Not Satisfiable", "Range Not Satisfiable\n");
                response.set_header("Content-Range", "bytes */" + std::to_string(size));
                add_static_headers(response, etag, last_modified);
                return response;
            }
            use_range = true;
        }
    }

    const auto first = use_range ? range->first : 0;
    const auto last = use_range ? range->last : (size == 0 ? 0 : size - 1);
    const auto length = use_range ? (last - first + 1) : size;

    http::HttpResponse response;
    if (use_range) {
        response.set_status(206, "Partial Content");
        response.set_header("Content-Range",
                            "bytes " + std::to_string(first) + "-" + std::to_string(last) + "/" +
                                std::to_string(size));
    } else {
        response.set_status(200, "OK");
    }
    response.set_header("Content-Type", mime_type_for_path(candidate));
    add_static_headers(response, etag, last_modified);

    if (request.method == "HEAD") {
        response.set_suppressed_body_length(static_cast<std::size_t>(length));
        return response;
    }

    const auto body = read_file_slice(candidate, first, length);
    if (!body) {
        return not_found();
    }
    response.set_body(*body);
    return response;
}

}  // namespace vhttp::static_files
