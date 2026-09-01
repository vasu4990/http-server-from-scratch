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
- parser fragmentation tests

## M2 — routing and request semantics ✅
- method-aware route trie
- static and parameterized paths
- query/path parameters
- `404` / `405`, deterministic `Allow`, `HEAD`, automatic `OPTIONS`

## M3 — HTTP/1.1 connection lifecycle ✅
- HTTP/1.1 persistence / HTTP/1.0 keep-alive opt-in
- multiple requests per socket
- pipelined-byte preservation
- request ceiling + receive idle timeout
- deterministic closure and loopback tests

## M4 — message framing ✅
- incremental chunked request decoder
- bounded extensions/trailers
- framing ambiguity hardening
- chunked response encoder
- adversarial and real TCP framing tests

## M5 — secure static file engine ✅
- document-root confinement and traversal/symlink escape checks
- MIME + nosniff
- ETag/date conditionals
- single ranges with `206` / `416`
- `GET` / `HEAD` parity and filesystem/TCP tests

## M6A — bounded blocking concurrency ✅
- fixed worker pool
- bounded accepted-connection queue
- queue-saturation rejection accounting
- timed accept polling for deterministic stop observation
- graceful queued/active drain and worker join
- runtime statistics and concurrency/backpressure tests

## M6B — Linux event runtime ← next
- nonblocking sockets
- `epoll` accept/read/write readiness
- per-connection parser/output state
- bounded output buffering and backpressure
- timer/deadline integration
- correctness parity tests against blocking/thread-pool runtimes

## M6C — Windows event runtime
- IOCP accept/read/write completion model
- per-connection operation lifetime rules
- cancellation/drain semantics
- correctness parity with Linux/event and blocking runtimes

## M7 — verification and performance
- fuzzing + sanitizers
- malformed-request corpus
- stress/load harness
- `wrk` benchmark harness
- latency percentiles and resource profiling
