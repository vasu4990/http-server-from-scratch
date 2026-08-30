# Milestone 2 Plan — Routing and Request Semantics

Milestone 2 moves request dispatch out of ad-hoc application conditionals and into a reusable routing layer.

## Scope

- Method-aware route registration.
- Static and parameterized path matching.
- Query-string parsing independent of route matching.
- Path parameter extraction.
- `404 Not Found` when no route path matches.
- `405 Method Not Allowed` when the path exists for other methods.
- `Allow` response header for 405 responses.
- `HEAD` fallback to matching `GET` routes while suppressing the response body on the wire.
- Automatic `OPTIONS` responses for registered paths.
- Deterministic route precedence: static segments before parameter segments.
- Unit tests for route matching and request target parsing.

## Non-goals

Persistent connections, chunked transfer coding, middleware, wildcard routes, URL decoding, and concurrency remain separate milestones so each layer can be validated independently.
