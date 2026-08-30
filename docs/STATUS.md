# Status

## Completed

### Milestone 1 — Blocking HTTP baseline
Cross-platform TCP sockets, incremental request parsing, request limits, `Content-Length` bodies, response serialization, tests, and Linux/Windows CI.

### Milestone 2 — Routing and request semantics
Method-aware route trie, static and parameterized paths, query parsing, `404`/`405`, deterministic `Allow`, `HEAD` fallback/body suppression, automatic `OPTIONS`, and router tests.

### Milestone 3 — HTTP/1.1 connection lifecycle
HTTP/1.1 persistent connections, HTTP/1.0 keep-alive opt-in, multiple requests per TCP socket, pipelined-byte preservation, configurable max requests, cross-platform idle receive timeouts, deterministic connection closure, and real loopback TCP integration tests.

All three completed milestones are verified with Linux GCC, Linux Clang, and Windows MSVC CI.

## Next

### Milestone 4 — Message framing
Implement incremental chunked request decoding, framing-ambiguity hardening, chunked response encoding, and adversarial fragmentation tests before moving to static-file serving.
