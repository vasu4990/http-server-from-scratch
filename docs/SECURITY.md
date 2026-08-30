# Security model

This repository is currently an educational implementation, not a production reverse proxy.

## Protections already present

### Parser and framing baseline

- Request-line byte limit.
- Aggregate header byte limit.
- Header-count limit.
- Request-body byte limit.
- Header-name token validation.
- Conflicting `Content-Length` rejection.
- Unsupported `Transfer-Encoding` is rejected rather than guessed.
- HTTP/1.0 and HTTP/1.1 versions are explicitly recognized.

### Connection lifecycle

- Configurable maximum requests per persistent connection.
- Configurable socket receive idle timeout on Windows and POSIX systems.
- Silent idle/partial connections are retired rather than held forever.
- HTTP/1.x `Connection` tokens are parsed as case-insensitive comma-separated tokens rather than substring-matched.
- Server-enforced closure cannot be overridden by a handler-supplied `Connection: keep-alive` response header.
- Parse failures receive a `400` response and deterministic connection close.

## Planned hardening

- Strict `Transfer-Encoding` / `Content-Length` framing rules.
- Incremental chunk decoder overflow checks and trailer policy.
- Distinct header/body total deadlines in addition to per-receive idle timeout.
- Broader slow-client defenses and global connection limits once concurrency is introduced.
- Request-target normalization and traversal prevention before static file serving.
- Fuzz targets for parser, chunk decoder, and request target.
- ASan/UBSan/TSan CI where the toolchain supports them.

Do not deploy the current milestone directly on an untrusted public network.
