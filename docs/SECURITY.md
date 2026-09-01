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
- Statuses that forbid message bodies (`1xx`, `204`, `304`) are serialized without payload framing/body bytes.

### Connection lifecycle

- Configurable maximum requests per persistent connection.
- Configurable socket receive idle timeout on Windows and POSIX systems.
- Silent idle/partial connections are retired rather than held forever.
- HTTP/1.x `Connection` tokens are parsed as case-insensitive comma-separated tokens rather than substring-matched.
- Server-enforced closure cannot be overridden by a handler-supplied `Connection: keep-alive` response header.
- Parse failures receive a `400` response and deterministic connection close.

### Static file serving

- A static handler is bound to one explicit document root and URL prefix.
- The configured document root is canonicalized and must exist as a directory.
- Query strings are excluded from filesystem lookup.
- Percent escapes are validated; encoded/raw path separators, backslashes, NUL, and control characters are rejected.
- Raw or decoded `.` / `..` path segments fail closed.
- The resolved candidate is canonicalized and checked to remain under the canonical document root before it can be served.
- Pre-existing symlink/reparse-style paths that resolve outside the document root fail the containment check.
- Only regular files are served; directory index behavior is explicit and configurable.
- Maximum static-file size bounds the current in-memory GET implementation.
- Static responses add `X-Content-Type-Options: nosniff`.
- Range parsing accepts one bounded range and fails unsupported/unsatisfiable forms with `416` rather than guessing.
- `HEAD` obtains metadata and representation length without reading file payload bytes.

## Known limitations / planned hardening

- Static file confinement currently follows a canonicalize-then-open design. It rejects traversal and stable symlink escapes, but it is **not race-free** if a hostile local actor can replace path components between validation and file open. A future hardening step should use descriptor/handle-relative opens with no-follow/reparse controls appropriate to POSIX and Windows.
- Static `GET` currently copies the selected file bytes into memory; there is no zero-copy/sendfile/mapped/overlapped file path yet.
- ETags are weak metadata validators rather than content hashes.
- Multiple byte ranges / multipart range responses are not implemented.
- Full grammar validation for chunk-extension names/values remains unnecessary while extensions are not exposed to handlers.
- Distinct header/body total deadlines in addition to per-receive idle timeout remain future hardening.
- Broader slow-client defenses and global connection limits are deferred to the concurrent runtime milestone.
- Fuzz targets for request parser, chunk decoder, request target, and static-path normalization are still planned.
- ASan/UBSan/TSan CI remains planned where the toolchain supports it.

Do not deploy the current milestone directly on an untrusted public network.
