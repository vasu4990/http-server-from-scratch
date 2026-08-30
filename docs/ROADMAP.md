# Roadmap

## M0 — repository foundation
- C++20 + CMake
- Linux GCC/Clang CI
- Windows MSVC CI
- architecture/security documentation

## M1 — blocking HTTP baseline (current)
- cross-platform TCP listener and stream RAII
- incremental request parser
- request-line + headers + Content-Length body
- response serializer
- hello-world server
- parser fragmentation tests

## M2 — routing and request semantics
- route table / trie
- path parameters
- query parsing
- 404/405 behavior
- HEAD/OPTIONS semantics

## M3 — HTTP/1.1 connection lifecycle
- persistent connections
- configurable idle timeout
- request limit per connection
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
- Linux epoll
- Windows IOCP

## M7 — verification and performance
- fuzzing
- sanitizers
- malformed-request corpus
- wrk benchmark harness
- latency percentiles and resource profiling
