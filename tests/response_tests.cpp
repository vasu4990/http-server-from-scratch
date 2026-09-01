#include "vhttp/http/response.hpp"

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

void serializes_close_by_default() {
    auto response = vhttp::http::HttpResponse::text(200, "OK", "hello");
    const std::string wire = response.serialize(false);

    expect(wire.starts_with("HTTP/1.1 200 OK\r\n"), "status line emitted");
    expect(wire.find("Content-Length: 5\r\n") != std::string::npos, "content length generated");
    expect(wire.find("Connection: close\r\n") != std::string::npos, "connection close generated");
    expect(wire.ends_with("\r\n\r\nhello"), "body serialized");
}

void final_connection_policy_replaces_handler_header() {
    auto response = vhttp::http::HttpResponse::text(200, "OK", "hello");
    response.set_header("cOnNeCtIoN", "keep-alive");
    const std::string wire = response.serialize(false);

    expect(wire.find("cOnNeCtIoN:") == std::string::npos,
           "handler Connection header should not be serialized independently");
    expect(wire.find("Connection: close\r\n") != std::string::npos,
           "transport close decision should be authoritative");
}

void head_suppression_keeps_representation_length() {
    auto response = vhttp::http::HttpResponse::text(200, "OK", "hello");
    const std::string wire = response.serialize(true, true);

    expect(wire.find("Content-Length: 5\r\n") != std::string::npos,
           "HEAD response should retain GET representation length");
    expect(wire.find("Connection: keep-alive\r\n") != std::string::npos,
           "keep-alive should be serialized");
    expect(wire.ends_with("\r\n\r\n"), "suppressed body should not be sent");
}

void head_can_supply_length_without_materializing_body() {
    vhttp::http::HttpResponse response;
    response.set_status(200, "OK");
    response.set_suppressed_body_length(1234);
    const std::string wire = response.serialize(true, true);

    expect(wire.find("Content-Length: 1234\r\n") != std::string::npos,
           "suppressed-body length should be used only for bodyless HEAD serialization");
    expect(wire.ends_with("\r\n\r\n"), "bodyless HEAD should terminate after headers");
}

void chunked_response_uses_transfer_coding_without_content_length() {
    auto response = vhttp::http::HttpResponse::text(200, "OK", "Wikipedia");
    response.set_header("Content-Length", "999");
    response.set_header("Transfer-Encoding", "gzip");
    response.set_chunked();

    const std::string wire = response.serialize(true);
    expect(wire.find("Transfer-Encoding: chunked\r\n") != std::string::npos,
           "chunked response should advertise chunked transfer coding");
    expect(wire.find("Content-Length:") == std::string::npos,
           "chunked response must not emit Content-Length");
    expect(wire.ends_with("\r\n\r\n9\r\nWikipedia\r\n0\r\n\r\n"),
           "chunked response should encode body and terminal zero chunk");
}

void content_length_is_computed_from_actual_body() {
    auto response = vhttp::http::HttpResponse::text(200, "OK", "abc");
    response.set_header("Content-Length", "999");
    const std::string wire = response.serialize(false);

    expect(wire.find("Content-Length: 3\r\n") != std::string::npos,
           "serializer should derive Content-Length from actual body");
    expect(wire.find("Content-Length: 999") == std::string::npos,
           "handler-provided framing length must not override encoder");
}

void chunked_head_suppresses_chunks_but_keeps_transfer_encoding_header() {
    auto response = vhttp::http::HttpResponse::text(200, "OK", "hello");
    response.set_chunked();
    const std::string wire = response.serialize(true, true);

    expect(wire.find("Transfer-Encoding: chunked\r\n") != std::string::npos,
           "HEAD can describe the transfer coding that GET would use");
    expect(wire.ends_with("\r\n\r\n"), "HEAD must not send chunk frames or payload");
}

void body_forbidden_statuses_do_not_emit_payload_framing_or_body() {
    auto response = vhttp::http::HttpResponse::text(304, "Not Modified", "must-not-leak");
    response.set_header("ETag", "W/\"abc\"");
    const std::string wire = response.serialize(true);

    expect(wire.starts_with("HTTP/1.1 304 Not Modified\r\n"), "304 status should serialize");
    expect(wire.find("Content-Length:") == std::string::npos,
           "304 should not invent a Content-Length unrelated to the selected representation");
    expect(wire.find("Transfer-Encoding:") == std::string::npos,
           "304 must not carry transfer coding for a message body");
    expect(wire.find("must-not-leak") == std::string::npos, "304 must not emit a payload body");
    expect(wire.ends_with("\r\n\r\n"), "304 should end after headers");
}

}  // namespace

int main() {
    serializes_close_by_default();
    final_connection_policy_replaces_handler_header();
    head_suppression_keeps_representation_length();
    head_can_supply_length_without_materializing_body();
    chunked_response_uses_transfer_coding_without_content_length();
    content_length_is_computed_from_actual_body();
    chunked_head_suppresses_chunks_but_keeps_transfer_encoding_header();
    body_forbidden_statuses_do_not_emit_payload_framing_or_body();
    std::cout << "response tests passed\n";
}
