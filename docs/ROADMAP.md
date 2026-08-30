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

## M3 — HTTP/1.1 connection lifecycle ← next
- persistent connections
- configurable idle timeout
- request limit per connection
- multiple requests per socket
- pipelined byte preservation
- graceful shutdown

## M4 — message framing
- chunked request decoder
- chunked response encoder
- framing ambiguity hardening

## M5 — static file engine
- path normalization
- MIME mapping
- ETag / If-None-Match
- Last-Modified
- byte ranges

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
