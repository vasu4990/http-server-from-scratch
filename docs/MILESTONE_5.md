# Milestone 5 — Secure Static File Engine

## Status

Implemented on `feat/static-file-engine` with Linux GCC/Clang and Windows MSVC verification required before merge.

## Delivered

### Document-root confinement

- `StaticFileHandler` maps one URL prefix to a configured document root.
- The document root is resolved at construction and must already exist as a directory.
- Request queries are excluded from filesystem lookup.
- Path segments are percent-decoded with malformed escapes rejected.
- Decoded `/`, `\\`, NUL, and control characters are rejected before filesystem APIs are called.
- `.` and `..` segments fail closed.
- Candidate paths are canonicalized and must remain beneath the canonical document root.
- Symlinks/reparse-point-style paths that resolve outside the root are rejected by the post-resolution containment check.
- Directory requests can map to one explicitly configured index filename.
- Only regular files are served.
- A configurable maximum file size bounds the current in-memory serving model.

### HTTP representation metadata

- Deterministic MIME mapping with `application/octet-stream` fallback.
- `X-Content-Type-Options: nosniff` on successful static representation responses.
- Weak deterministic ETags derived from file size and modification metadata.
- `Last-Modified` generation.
- `If-None-Match` handling including `*` and weak comparison.
- `If-Modified-Since` handling when `If-None-Match` is absent.
- Bodyless `304 Not Modified` serialization.

### Byte ranges

- One `bytes=` range per request.
- Closed ranges (`bytes=2-5`).
- Open-ended ranges (`bytes=7-`).
- Suffix ranges (`bytes=-3`).
- `206 Partial Content` with exact `Content-Range`.
- Unsatisfiable/unsupported multi-ranges fail with `416 Range Not Satisfiable` and `Content-Range: bytes */size`.
- `If-Range` date validation is supported. The current generated ETag is weak, so it is deliberately not accepted as an `If-Range` validator.

### GET / HEAD parity

- `HEAD` resolves the same file metadata and range selection as `GET`.
- `HEAD` does not read the file payload into memory.
- `HttpResponse` can carry a suppressed representation length so the serializer emits the correct `Content-Length` while sending no body.
- Statuses that forbid message bodies (`1xx`, `204`, `304`) no longer emit payload framing or payload bytes.

## Verification

Unit tests cover:

- prefix fallthrough and explicit `405` method policy.
- directory index serving.
- MIME mapping and safe percent-decoded filenames.
- malformed escapes, encoded separators, Windows separators, raw/encoded traversal, and symlink escape attempts.
- ETag and modification-date conditional requests.
- bodyless `HEAD` with correct representation length.
- closed, open-ended, and suffix ranges.
- `416` behavior and multi-range rejection.
- `If-Range` fallback behavior.

A real loopback TCP integration test pipelines full `GET`, `HEAD`, `206`, `304`, and `416` requests across one persistent connection and verifies response status/framing/body behavior.

## Important limitations

The current implementation deliberately favors correctness and observability over throughput:

- file payloads for `GET` are copied into memory (bounded by `max_file_bytes`); there is no `sendfile`, mapped-file, overlapped, or streaming path yet.
- only a single byte range is supported; multipart ranges are not implemented.
- ETags are weak metadata validators, not content hashes.
- canonicalize-then-open confinement protects normal traversal and pre-existing symlink escapes, but the implementation is not race-free against an attacker concurrently replacing filesystem objects between validation and open. Descriptor/handle-relative race-resistant file opening is future hardening.
- the server accept loop itself is still serial/blocking; scalable runtimes are Milestone 6.
