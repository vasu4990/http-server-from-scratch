# Security model

This repository is currently an educational implementation, not a production reverse proxy.

## Parser protections already present

- Request-line byte limit.
- Aggregate header byte limit.
- Header-count limit.
- Request-body byte limit.
- Header-name token validation.
- Conflicting `Content-Length` rejection.
- Unsupported `Transfer-Encoding` is rejected rather than guessed.
- HTTP/1.0 and HTTP/1.1 versions are explicitly recognized.

## Planned hardening

- Strict `Transfer-Encoding` / `Content-Length` framing rules.
- Chunk decoder overflow checks.
- Header and body read deadlines.
- Slow-client defenses.
- Request-target normalization and traversal prevention.
- Fuzz targets for parser, chunk decoder, and request target.
- ASan/UBSan/TSan CI where the toolchain supports them.

Do not deploy the current milestone directly on an untrusted public network.
