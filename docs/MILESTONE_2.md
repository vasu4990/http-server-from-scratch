# Milestone 2 — Routing and Request Semantics

Milestone 2 replaces application-level `if` chains with a reusable method-aware route trie.

## Delivered

- Route registration for `GET`, `HEAD`, `POST`, `PUT`, `PATCH`, `DELETE`, and `OPTIONS`.
- Static and parameterized route segments such as `/users/:id`.
- Static-route precedence over parameter routes at the same depth.
- Path-parameter extraction through `HttpRequest::path_param()`.
- Query-string parsing with repeated keys preserved.
- Query lookup through `HttpRequest::query()`.
- `404 Not Found` for unknown paths.
- `405 Method Not Allowed` for known paths with unsupported methods.
- Deterministic `Allow` response headers.
- `HEAD` fallback to matching `GET` handlers while preserving `Content-Length` and suppressing the body on the wire.
- Automatic `OPTIONS` responses for registered paths.
- Duplicate and ambiguous route-registration protection.
- Dedicated router tests plus end-to-end manual HTTP checks.

## Verification

The milestone passes the full CTest suite locally and on GitHub Actions with Linux GCC, Linux Clang, and Windows MSVC. Manual socket-level server checks verified parameterized GET, HEAD, OPTIONS, and 405 behavior.

## Intentionally deferred

- Percent-decoding and request-target canonicalization.
- Wildcard routes.
- Middleware.
- Persistent connections and pipelining.
- Chunked transfer coding.

Those remain isolated follow-up milestones so protocol behavior can be validated incrementally.
