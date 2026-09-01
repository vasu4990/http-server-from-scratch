# Architecture

## Current blocking architecture

```text
Client
  |
  v
TcpListener --accept--> TcpStream
                        |
                        +--> SO_RCVTIMEO idle guard
                        |
                        v
                 HttpRequestParser
                  /             \
          Content-Length      chunked
                  |              |
                  |       size -> data -> CRLF
                  |              |
                  |       zero chunk -> trailers
                  \             /
                        v
                    HttpRequest
                 body + trailers
                        |
                        v
               Connection policy
                        |
                        v
              Application dispatcher
                /               \
               /                 \
     StaticFileHandler       Method-aware Router
       |     |     |                |
       |     |     |                v
   path   validators ranges       Handler
   jail        |      |             |
      \        |     /              |
       \       |    /               |
        +------v---+----------------+
               |
               v
           HttpResponse
               |
        +------+------+
        |             |
 Content-Length     chunked
        |             |
        +------v------+
               |
   Authoritative wire serializer
               |
        +------+------+
        |             |
    keep alive       close
        |             |
        v             v
 preserved bytes   retire socket
 / next recv
```

The runtime intentionally remains blocking and accepts one connection at a time. This keeps HTTP state-machine, framing, filesystem semantics, and lifecycle correctness observable before thread pools or event-driven I/O are introduced.

## Design boundaries

- `vhttp::net` owns OS socket differences (Winsock2 vs POSIX), connected streams, client connects, listener ports, and socket-level receive timeouts.
- `vhttp::http` owns HTTP syntax, request parsing, message framing, request/response wire models, trailers, and serialization.
- `vhttp::router` owns request-target interpretation and method-aware application routing.
- `vhttp::static_files` owns URL-prefix matching, static path decoding/confinement, representation metadata, conditional requests, MIME mapping, and single-range selection.
- `vhttp::server` owns HTTP connection lifecycle decisions and connects transport, parser, application dispatcher, and serializer.
- TCP read boundaries are never treated as HTTP message boundaries.
- Parsed bytes beyond one complete request remain owned by the parser until the server explicitly moves them into the next parser cycle.

## Message framing invariants

1. A request cannot use both `Transfer-Encoding` and `Content-Length`.
2. The current transfer decoder accepts exactly one supported HTTP/1.1 coding: `chunked`.
3. Chunk size is parsed as bounded hexadecimal with overflow and decoded-body-budget checks.
4. Non-zero chunk data must be followed by CRLF.
5. A zero-size chunk transitions into a bounded trailer section; the empty trailer line completes the message.
6. Framing/routing-sensitive fields are rejected from trailers.
7. Bytes after the final trailer CRLF remain available for the next request on the connection.
8. Response framing headers are serializer-authoritative and statuses that forbid bodies do not emit payload framing/body bytes.

## Static file invariants

1. Filesystem lookup begins only after the request path is matched to the configured URL prefix.
2. Percent-decoded segments may not introduce separators, NUL/control bytes, or `.` / `..` traversal.
3. The candidate is canonicalized and must remain beneath the canonical document root.
4. A symlink/reparse-style path that already resolves outside the root cannot be served.
5. Only regular files are served; index-file behavior is explicit.
6. `GET` payload reads are bounded by `max_file_bytes`; a range reads only the selected slice.
7. `HEAD` reuses representation metadata/range selection but does not read the payload.
8. Conditional validators run before payload I/O; a matching validator returns bodyless `304`.
9. Only one byte range is accepted in this milestone; unsupported/unsatisfiable ranges return `416` with the complete size.
10. Canonicalize-then-open is not claimed to be race-free against hostile concurrent filesystem mutation.

## Persistent connection invariants

1. HTTP/1.1 persists unless a `Connection: close` token is present.
2. HTTP/1.0 closes unless a `Connection: keep-alive` token is present.
3. A response-level close request, configured request ceiling, parse failure, peer EOF, or idle timeout terminates the connection.
4. Pipelined bytes are processed before another blocking `recv()` call.
5. Responses remain ordered because the current runtime dispatches one request at a time.
6. The serializer emits the final connection decision exactly once; application handlers cannot contradict a server-enforced close.

## Verification boundary

`Server::serve_connection()` accepts an already-connected `TcpStream`. Production `listen_and_serve()` uses it after `accept()`, while integration tests create real loopback listener/client pairs and exercise that same connection implementation. Connection tests cover pipelining and chunked framing; static-file integration additionally pipelines full `GET`, `HEAD`, `206`, `304`, and `416` responses on one persistent connection.

## Planned evolution

1. ✅ Baseline socket + parser + response serializer.
2. ✅ Router and method/request semantics.
3. ✅ Persistent connections, pipelined-byte preservation, request ceilings, and idle retirement.
4. ✅ Chunked transfer decoding/encoding and framing hardening.
5. ✅ Static files, conditionals, single ranges, and document-root confinement.
6. Bounded thread-pool runtime, graceful drain, then Linux `epoll` and Windows IOCP backends.
7. Fuzzing, sanitizers, stress tests, and reproducible benchmarks.
