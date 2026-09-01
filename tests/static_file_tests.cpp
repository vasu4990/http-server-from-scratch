#include "vhttp/static_files/static_file_handler.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

class TempWorkspace {
public:
    TempWorkspace() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("vhttp-static-" + std::to_string(nonce));
        root = path / "root";
        std::filesystem::create_directories(root / "nested");
    }

    ~TempWorkspace() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
    std::filesystem::path root;
};

void write_file(const std::filesystem::path& path, std::string_view body) {
    std::ofstream out(path, std::ios::binary);
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    if (!out) {
        throw std::runtime_error("failed to create static-file test fixture");
    }
}

vhttp::http::HttpRequest request(std::string method, std::string target) {
    vhttp::http::HttpRequest value;
    value.method = std::move(method);
    value.target = std::move(target);
    value.version = "HTTP/1.1";
    return value;
}

vhttp::static_files::StaticFileHandler make_handler(const TempWorkspace& workspace) {
    vhttp::static_files::StaticFileConfig config;
    config.document_root = workspace.root;
    config.url_prefix = "/static";
    return vhttp::static_files::StaticFileHandler(std::move(config));
}

void serves_files_index_percent_decoding_and_mime() {
    TempWorkspace workspace;
    write_file(workspace.root / "index.html", "<h1>index</h1>");
    write_file(workspace.root / "hello world.txt", "hello world");
    write_file(workspace.root / "nested" / "data.json", "{\"ok\":true}");
    const auto handler = make_handler(workspace);

    expect(!handler.try_serve(request("GET", "/other/file.txt")).has_value(),
           "prefix mismatch should fall through to the next handler");

    const auto index = handler.try_serve(request("GET", "/static"));
    expect(index && index->status == 200 && index->body == "<h1>index</h1>",
           "static prefix root should serve configured index file");
    expect(index->headers.at("Content-Type") == "text/html; charset=utf-8", "HTML MIME type should be detected");

    const auto spaced = handler.try_serve(request("GET", "/static/hello%20world.txt?download=1"));
    expect(spaced && spaced->status == 200 && spaced->body == "hello world",
           "safe percent-decoded filename should be served and query ignored");
    expect(spaced->headers.at("Content-Type") == "text/plain; charset=utf-8", "text MIME type should be detected");

    const auto nested = handler.try_serve(request("GET", "/static/nested/data.json"));
    expect(nested && nested->headers.at("Content-Type") == "application/json; charset=utf-8",
           "nested file should be served with JSON MIME type");
    expect(nested->headers.at("X-Content-Type-Options") == "nosniff", "static responses should opt out of MIME sniffing");
}

void traversal_and_separator_smuggling_fail_closed() {
    TempWorkspace workspace;
    write_file(workspace.root / "safe.txt", "safe");
    write_file(workspace.path / "outside.txt", "secret");
    const auto handler = make_handler(workspace);

    const auto raw = handler.try_serve(request("GET", "/static/../outside.txt"));
    expect(raw && raw->status == 404, "raw dot-dot traversal should fail closed");

    const auto encoded = handler.try_serve(request("GET", "/static/%2e%2e/outside.txt"));
    expect(encoded && encoded->status == 404, "percent-encoded dot-dot traversal should fail closed");

    const auto encoded_slash = handler.try_serve(request("GET", "/static/nested%2Foutside.txt"));
    expect(encoded_slash && encoded_slash->status == 400, "encoded path separators should be rejected before filesystem lookup");

    const auto bad_escape = handler.try_serve(request("GET", "/static/file%2.txt"));
    expect(bad_escape && bad_escape->status == 400, "malformed percent escape should be rejected");

    const auto backslash = handler.try_serve(request("GET", "/static/foo\\bar.txt"));
    expect(backslash && backslash->status == 400, "Windows-style separator smuggling should be rejected");

    std::error_code ec;
    std::filesystem::create_symlink(workspace.path / "outside.txt", workspace.root / "escape.txt", ec);
    if (!ec) {
        const auto escaped = handler.try_serve(request("GET", "/static/escape.txt"));
        expect(escaped && escaped->status == 404, "symlink resolving outside document root should not be served");
    }
}

void supports_conditionals_and_head_without_reading_body() {
    TempWorkspace workspace;
    write_file(workspace.root / "asset.txt", "0123456789");
    const auto handler = make_handler(workspace);

    const auto initial = handler.try_serve(request("GET", "/static/asset.txt"));
    expect(initial && initial->status == 200, "initial GET should succeed");
    const auto etag = initial->headers.at("ETag");
    const auto last_modified = initial->headers.at("Last-Modified");

    auto inm_request = request("GET", "/static/asset.txt");
    inm_request.headers.emplace("if-none-match", etag);
    const auto not_modified = handler.try_serve(inm_request);
    expect(not_modified && not_modified->status == 304 && not_modified->body.empty(),
           "matching If-None-Match should produce bodyless 304");
    const auto not_modified_wire = not_modified->serialize(true);
    expect(not_modified_wire.find("Content-Length:") == std::string::npos,
           "304 serializer should not invent a zero representation length");
    expect(not_modified_wire.ends_with("\r\n\r\n"), "304 should terminate after headers");

    auto ims_request = request("GET", "/static/asset.txt");
    ims_request.headers.emplace("if-modified-since", last_modified);
    const auto ims = handler.try_serve(ims_request);
    expect(ims && ims->status == 304, "matching If-Modified-Since should produce 304");

    const auto head = handler.try_serve(request("HEAD", "/static/asset.txt"));
    expect(head && head->status == 200 && head->body.empty(), "HEAD handler should avoid reading the file body");
    const auto head_wire = head->serialize(true, true);
    expect(head_wire.find("Content-Length: 10\r\n") != std::string::npos,
           "HEAD should retain the GET representation length without materializing the body");
    expect(head_wire.ends_with("\r\n\r\n"), "HEAD wire response should contain no payload");
}

void supports_single_ranges_and_if_range() {
    TempWorkspace workspace;
    write_file(workspace.root / "asset.txt", "0123456789");
    const auto handler = make_handler(workspace);

    auto ranged_request = request("GET", "/static/asset.txt");
    ranged_request.headers.emplace("range", "bytes=2-5");
    const auto ranged = handler.try_serve(ranged_request);
    expect(ranged && ranged->status == 206 && ranged->body == "2345", "closed byte range should produce requested slice");
    expect(ranged->headers.at("Content-Range") == "bytes 2-5/10", "206 should include exact Content-Range");

    auto suffix_request = request("GET", "/static/asset.txt");
    suffix_request.headers.emplace("range", "bytes=-3");
    const auto suffix = handler.try_serve(suffix_request);
    expect(suffix && suffix->status == 206 && suffix->body == "789", "suffix byte range should be supported");

    auto open_request = request("GET", "/static/asset.txt");
    open_request.headers.emplace("range", "bytes=7-");
    const auto open = handler.try_serve(open_request);
    expect(open && open->status == 206 && open->body == "789", "open-ended byte range should be supported");

    auto invalid_request = request("GET", "/static/asset.txt");
    invalid_request.headers.emplace("range", "bytes=99-100");
    const auto invalid = handler.try_serve(invalid_request);
    expect(invalid && invalid->status == 416, "unsatisfiable byte range should return 416");
    expect(invalid->headers.at("Content-Range") == "bytes */10", "416 should expose current representation size");

    auto multiple_request = request("GET", "/static/asset.txt");
    multiple_request.headers.emplace("range", "bytes=0-1,4-5");
    const auto multiple = handler.try_serve(multiple_request);
    expect(multiple && multiple->status == 416, "multiple ranges are intentionally rejected in this milestone");

    auto if_range_request = request("GET", "/static/asset.txt");
    if_range_request.headers.emplace("range", "bytes=2-5");
    if_range_request.headers.emplace("if-range", "\"different-strong-validator\"");
    const auto if_range = handler.try_serve(if_range_request);
    expect(if_range && if_range->status == 200 && if_range->body == "0123456789",
           "mismatching If-Range should fall back to the complete representation");
}

void method_policy_is_explicit() {
    TempWorkspace workspace;
    write_file(workspace.root / "asset.txt", "data");
    const auto handler = make_handler(workspace);

    const auto response = handler.try_serve(request("POST", "/static/asset.txt"));
    expect(response && response->status == 405, "matched static path should reject non-GET/HEAD methods");
    expect(response->headers.at("Allow") == "GET, HEAD", "405 should advertise supported static methods");
}

}  // namespace

int main() {
    serves_files_index_percent_decoding_and_mime();
    traversal_and_separator_smuggling_fail_closed();
    supports_conditionals_and_head_without_reading_body();
    supports_single_ranges_and_if_range();
    method_policy_is_explicit();
    std::cout << "static file tests passed\n";
}
