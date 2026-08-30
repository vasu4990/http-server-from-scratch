# Milestone 3 — HTTP/1.1 Connection Lifecycle

## Status

Implemented and CI-verified on Linux (GCC + Clang) and Windows (MSVC).

## Delivered

- HTTP/1.1 persistent connections by default unless `Connection: close` is present.
- HTTP/1.0 non-persistent behavior by default with explicit `Connection: keep-alive` opt-in.
- Case-insensitive, comma-token-aware `Connection` parsing.
- Multiple sequential HTTP requests per accepted TCP socket.
- Preservation and immediate reuse of bytes that belong to a pipelined next request.
- Configurable `max_requests_per_connection` safety limit.
- Cross-platform receive idle timeout using `SO_RCVTIMEO`.
- Explicit socket-timeout classification instead of treating idle retirement as a generic connection failure.
- Server-authoritative `Connection` response serialization so handler headers cannot contradict transport lifecycle decisions.
- TCP client connect support and ephemeral listener-port discovery for real loopback integration testing.
- Public `serve_connection()` entry point for deterministic single-connection embedding/testing.

## Verification

Dedicated connection-policy tests cover:

- HTTP/1.1 default persistence.
- HTTP/1.1 `close` precedence.
- HTTP/1.0 default close behavior.
- HTTP/1.0 keep-alive opt-in.
- case-insensitive response close directives.
- exact comma-separated connection-token parsing.

Loopback TCP integration tests cover:

- two pipelined HTTP/1.1 requests sent in one TCP write.
- two ordered HTTP responses on the same socket.
- first-response keep-alive and second-request explicit close.
- max-request limit forcing a deterministic close.
- idle connection retirement without emitting a fabricated HTTP response.

The integration tests use the real cross-platform TCP layer rather than a mocked byte stream.

## Important limitation

The runtime is still intentionally blocking and handles accepted connections serially in `listen_and_serve()`. Persistent connections are implemented, but scalable concurrency is deferred to later thread-pool/`epoll`/IOCP milestones.

Chunked transfer coding is also intentionally not implemented yet; message-framing work is Milestone 4.
