# Status

## Completed

### Milestone 1 — Blocking HTTP baseline
Cross-platform TCP sockets, incremental parsing, bounded `Content-Length` bodies, response serialization, tests, and cross-platform CI.

### Milestone 2 — Routing and request semantics
Method-aware route trie, path/query parameters, `404`/`405`, deterministic `Allow`, `HEAD`, automatic `OPTIONS`, and router tests.

### Milestone 3 — HTTP/1.1 connection lifecycle
Persistent HTTP/1.1, HTTP/1.0 keep-alive opt-in, pipelining carryover, request/idle limits, deterministic closure, and real TCP tests.

### Milestone 4 — HTTP message framing
Chunked decoding/encoding, bounded extensions/trailers, ambiguity hardening, authoritative response framing, adversarial tests, and real TCP coverage.

### Milestone 5 — Secure static file engine
Document-root confinement, strict path decoding, MIME/validators/ranges, HEAD parity, traversal/symlink checks, filesystem tests, and real TCP coverage.

### Milestone 6A — Bounded thread-pool runtime
Fixed workers, bounded accepted-connection queue, saturation rejection accounting, timed accept polling, graceful drain/join, runtime statistics, concurrency tests, and queue-pressure tests.

The implementation head for Milestone 6A passed Linux GCC, Linux Clang, and Windows MSVC before the final documentation/PR verification gate.

## Next

### Milestone 6B — Linux `epoll` runtime
Build a nonblocking Linux backend with explicit per-connection input/output state, readiness-driven reads/writes, deadlines, backpressure, and behavior-parity tests against the already-verified blocking/thread-pool stack.
