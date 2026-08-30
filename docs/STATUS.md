# Status

## Completed

### Milestone 1 — Blocking HTTP baseline
Cross-platform TCP sockets, incremental request parsing, request limits, `Content-Length` bodies, response serialization, tests, and Linux/Windows CI.

### Milestone 2 — Routing and request semantics
Method-aware route trie, static and parameterized paths, query parsing, `404`/`405`, deterministic `Allow`, `HEAD` fallback/body suppression, automatic `OPTIONS`, and router tests.

Both completed milestones are verified with Linux GCC, Linux Clang, and Windows MSVC CI.

## Next

### Milestone 3 — HTTP/1.1 connection lifecycle
Persistent connections, multiple requests per socket, pipelined-byte preservation, request/idle limits, and graceful connection shutdown.
