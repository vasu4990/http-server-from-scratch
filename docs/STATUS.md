# Status

## Completed

### Milestone 1 — Blocking HTTP baseline
Cross-platform TCP sockets, incremental request parsing, request limits, `Content-Length` bodies, response serialization, tests, and Linux/Windows CI.

### Milestone 2 — Routing and request semantics
Method-aware route trie, static and parameterized paths, query parsing, `404`/`405`, deterministic `Allow`, `HEAD` fallback/body suppression, automatic `OPTIONS`, and router tests.

### Milestone 3 — HTTP/1.1 connection lifecycle
HTTP/1.1 persistent connections, HTTP/1.0 keep-alive opt-in, multiple requests per TCP socket, pipelined-byte preservation, configurable max requests, cross-platform idle receive timeouts, deterministic connection closure, and real loopback TCP integration tests.

### Milestone 4 — HTTP message framing
Chunked request decoding, chunk extensions, bounded trailers, framing ambiguity rejection, decoded-body/line/trailer limits, chunked response encoding, authoritative framing headers, adversarial tests, and real TCP chunked+pipelined integration coverage.

### Milestone 5 — Secure static file engine
Document-root-confined file lookup, strict percent-decoding/path checks, MIME mapping, weak ETags, Last-Modified conditionals, single byte ranges with `206`/`416`, `HEAD` metadata parity without payload reads, symlink escape checks, filesystem tests, and real TCP static-file integration coverage.

Milestones 1–5 are expected to be merged only after Linux GCC, Linux Clang, and Windows MSVC CI pass on their final heads.

## Next

### Milestone 6 — Scalable runtimes
Introduce bounded concurrency without changing HTTP semantics: fixed worker pool first, graceful stop/drain and connection backpressure, then Linux `epoll` and Windows IOCP backends with cross-runtime verification.
