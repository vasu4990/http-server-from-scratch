#pragma once

#include "vhttp/http/request.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace vhttp::http {

enum class ParseStatus {
    need_more_data,
    complete,
    error,
};

struct ParserLimits {
    std::size_t max_request_line_bytes = 8 * 1024;
    std::size_t max_header_bytes = 32 * 1024;
    std::size_t max_header_count = 100;
    std::size_t max_body_bytes = 10 * 1024 * 1024;
    std::size_t max_chunk_line_bytes = 1024;
    std::size_t max_trailer_bytes = 16 * 1024;
    std::size_t max_trailer_count = 50;
};

class HttpRequestParser {
public:
    explicit HttpRequestParser(ParserLimits limits = {});

    ParseStatus feed(std::string_view bytes);
    [[nodiscard]] const HttpRequest& request() const noexcept { return request_; }
    [[nodiscard]] const std::string& error_message() const noexcept { return error_message_; }
    [[nodiscard]] bool complete() const noexcept { return state_ == State::complete; }
    [[nodiscard]] std::string take_remaining();
    void reset();

private:
    enum class State {
        request_line,
        headers,
        content_length_body,
        chunk_size,
        chunk_data,
        chunk_data_crlf,
        trailers,
        complete,
        error,
    };

    ParseStatus parse_available();
    bool parse_request_line(std::string_view line);
    bool parse_header_line(std::string_view line);
    bool parse_trailer_line(std::string_view line);
    bool parse_chunk_size_line(std::string_view line);
    bool finalize_headers();
    void fail(std::string message);

    ParserLimits limits_;
    State state_ = State::request_line;
    HttpRequest request_;
    std::string buffer_;
    std::string error_message_;
    std::size_t header_bytes_ = 0;
    std::size_t header_count_ = 0;
    std::size_t trailer_bytes_ = 0;
    std::size_t trailer_count_ = 0;
    std::size_t current_chunk_remaining_ = 0;
    std::optional<std::size_t> content_length_;
    bool chunked_ = false;
};

}  // namespace vhttp::http
