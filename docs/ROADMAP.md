# Roadmap

## M0 — repository foundation ✅
- C++20 + CMake
- Linux GCC/Clang CI
- Windows MSVC CI
- architecture/security documentation

## M1 — blocking HTTP baseline ✅
- cross-platform TCP listener and stream RAII
- incremental request parser
- request-line + headers + `Content-Length` body
- response serializer
- hello-world server
- parser fragmentation tests

## M2 — routing and request semantics ✅
- method-aware route trie
- static and parameterized paths
- path parameter extraction
- query parsing with repeated-key preservation
- `404` / `405` behavior and deterministic `Allow`
- `HEAD` fallback with wire-level body suppression
- automatic `OPTIONS`
- router unit tests

## M3 — HTTP/1.1 connection lifecycle ✅
- HTTP/1.1 default persistence and HTTP/1.0 keep-alive opt-in
- configurable idle receive timeout
- request limit per connection
- multiple requests per socket
- pipelined-byte preservation across parser resets
- deterministic `Connection: close` behavior
- real loopback TCP integration tests

## M4 — message framing ✅
- incremental chunked request decoder
- bounded chunk extensions and trailer parsing
- independent chunk/trailer limits
- chunked response encoder
- serializer-authoritative message framing
- `Transfer-Encoding` / `Content-Length` ambiguity hardening
- fragmentation/adversarial framing tests
- real TCP chunked + pipelined integration test

## M5 — static file engine ← next
- canonical path normalization and document-root confinement
- MIME mapping
- ETag / If-None-Match
- Last-Modified / If-Modified-Since
- byte ranges and `206`/`416`
- `HEAD` parity and traversal/symlink hardening

## M6 — scalable runtimes
- fixed thread pool
- Linux `epoll`
- Windows IOCP

## M7 — verification and performance
- fuzzing
- sanitizers
- malformed-request corpus
- `wrk` benchmark harness
- latency percentiles and resource profiling
