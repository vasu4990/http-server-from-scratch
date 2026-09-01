# Milestone 4 — HTTP Message Framing

## Status

Implemented and CI-verified on Linux (GCC + Clang) and Windows (MSVC).

## Delivered

### Chunked request decoding

- Incremental state-machine support for `Transfer-Encoding: chunked` requests.
- Chunk-size parsing in hexadecimal with overflow detection.
- Chunk-size line limits, including extension bytes.
- Bounded chunk extensions: accepted but intentionally not interpreted by the application layer.
- Exact chunk-data length enforcement and mandatory CRLF validation after every non-zero chunk.
- Terminal zero-chunk handling.
- Trailer parsing with independent byte/count limits.
- Case-insensitive trailer lookup through `HttpRequest::trailer()`.
- Conservative rejection of framing/routing-sensitive trailer names such as `Content-Length`, `Transfer-Encoding`, `Host`, `Connection`, and `Trailer`.
- Preservation of bytes after the trailer terminator so a pipelined next request is not lost.

### Framing ambiguity hardening

- Requests containing both `Transfer-Encoding` and `Content-Length` are rejected.
- HTTP/1.0 chunked requests are rejected.
- Unsupported transfer-coding chains are rejected instead of partially decoded.
- Duplicate/combined transfer-coding values that are not exactly one `chunked` coding are rejected.
- Decoded body limits are checked from the chunk-size line before the chunk is accepted.
- Invalid hexadecimal sizes, size overflow, malformed chunk terminators, oversized chunk lines, and forbidden trailers fail closed.

### Chunked response encoding

- `HttpResponse::set_chunked()` enables chunked transfer framing.
- The current encoder emits the in-memory body as one data chunk followed by the terminal zero chunk.
- `Content-Length` and `Transfer-Encoding` are serializer-authoritative; handlers cannot inject contradictory framing headers.
- `HEAD` suppresses chunk frames/body while retaining the transfer-coding metadata that the corresponding response would use.

## Verification

Dedicated framing tests cover:

- chunked bodies fragmented across parser feeds.
- chunk extensions.
- trailer parsing and lookup.
- bytes following a terminal chunk/trailer block.
- `Transfer-Encoding` + `Content-Length` ambiguity.
- unsupported coding chains.
- HTTP/1.0 transfer-coding rejection.
- invalid and overflowing chunk sizes.
- missing post-chunk CRLF.
- decoded-body limits.
- chunk-line limits.
- forbidden trailer fields.
- malformed empty chunk extensions.

Response tests cover authoritative `Content-Length`, chunked wire encoding, terminal zero chunks, conflicting handler framing headers, and `HEAD` suppression.

Real loopback TCP integration verifies a chunked POST with a trailer followed immediately by a pipelined GET on the same connection. The handler receives the decoded body/trailer, emits a chunked response, and the following request is still dispatched correctly.

## Current limitation

The parser assembles the decoded request body in memory and the chunked response encoder currently frames one in-memory body as a single data chunk. Streaming body callbacks/backpressure are intentionally deferred until the scalable runtime work; configured body limits bound the current memory exposure.
