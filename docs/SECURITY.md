# Security model

This repository is currently an educational implementation, not a production reverse proxy.

## Protections already present

### Request syntax and bounded parsing

- Request-line byte limit.
- Aggregate header byte limit.
- Header-count limit.
- Request-body byte limit.
- Header-name token validation.
- Conflicting `Content-Length` rejection.
- HTTP/1.0 and HTTP/1.1 versions are explicitly recognized.

### Message framing

- `Transfer-Encoding` plus `Content-Length` is rejected rather than resolved heuristically.
- Only one supported request transfer coding (`chunked`) is accepted; unsupported/combined coding chains fail closed.
- HTTP/1.0 chunked requests are rejected.
- Chunk sizes are hexadecimal-validated with integer-overflow checks.
- Declared chunk sizes are checked against the decoded body budget before acceptance.
- Chunk-size/extension lines have a configurable byte limit.
- Every non-zero chunk requires an exact trailing CRLF.
- Trailer bytes and trailer count have independent limits.
- Framing/routing-sensitive trailer fields (`Content-Length`, `Transfer-Encoding`, `Host`, `Connection`, `Trailer`) are rejected.
- Bytes after the terminal trailer block are preserved as a possible next HTTP request rather than discarded or misclassified.
- Response `Content-Length` / `Transfer-Encoding` are emitted by the serializer from the actual chosen encoder; handler-supplied contradictory framing headers are suppressed.

### Connection lifecycle

- Configurable maximum requests per persistent connection.
- Configurable socket receive idle timeout on Windows and POSIX systems.
- Silent idle/partial connections are retired rather than held forever.
- HTTP/1.x `Connection` tokens are parsed as case-insensitive comma-separated tokens rather than substring-matched.
- Server-enforced closure cannot be overridden by a handler-supplied `Connection: keep-alive` response header.
- Parse failures receive a `400` response and deterministic connection close.

## Planned hardening

- Full grammar validation for chunk-extension names/values if extension data becomes application-visible.
- Distinct header/body total deadlines in addition to per-receive idle timeout.
- Broader slow-client defenses and global connection limits once concurrency is introduced.
- Canonical request-target/path normalization and document-root confinement for static file serving.
- Symlink/reparse-point escape policy for static files.
- Fuzz targets for request parser, chunk decoder, and request target.
- ASan/UBSan/TSan CI where the toolchain supports them.

Do not deploy the current milestone directly on an untrusted public network.
