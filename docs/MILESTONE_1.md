# Milestone 1 — Blocking HTTP Baseline

Milestone 1 establishes the first end-to-end server path from a TCP connection to a parsed HTTP request and serialized HTTP response.

## Delivered

- Cross-platform socket runtime: Winsock2 on Windows and POSIX sockets on Linux.
- RAII `TcpListener` and `TcpStream` wrappers.
- Incremental HTTP/1.0 and HTTP/1.1 request parser.
- Correct parsing across arbitrary TCP fragmentation boundaries.
- Case-insensitive request-header lookup.
- `Content-Length` framed request bodies.
- Explicit rejection of unsupported `Transfer-Encoding` in this milestone.
- Request line, header-byte, header-count, and body-size limits.
- Conflicting `Content-Length` rejection.
- HTTP/1.1 response serialization with generated `Content-Length`.
- Minimal blocking server and hello-world example.
- Parser and serializer tests.
- Linux GCC/Clang CI and Windows MSVC CI.

## Verification

The milestone has been built and tested locally and on GitHub Actions across Linux GCC, Linux Clang, and Windows MSVC.

## Intentionally deferred

- Persistent connections.
- Chunked transfer coding.
- Routing and path parameters.
- Static file serving.
- Concurrent/event-driven runtime.
- Fuzzing and benchmark evidence.

These are tracked by later milestones rather than being hidden behind unsupported claims.
