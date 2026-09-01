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
              /                 \
     HTTP/1.1 default       HTTP/1.0 default
       keep-alive               close
              \                 /
                        |
                        v
                Method-aware Router
                        |
                        v
                     Handler
                        |
                        v
                   HttpResponse
                        |
              +---------+---------+
              |                   |
       Content-Length          chunked
              |                   |
              +---------+---------+
                        v
           Authoritative wire serializer
                        |
             +----------+----------+
             |                     |
         keep alive               close
             |                     |
             v                     v
 take parser remaining       retire socket
 bytes -> reset -> feed
             |
             `------> next request on same TcpStream
```

The runtime intentionally remains blocking and accepts one connection at a time. This keeps HTTP state-machine, framing, and lifecycle correctness observable before thread pools or event-driven I/O are introduced.

## Design boundaries

- `vhttp::net` owns OS socket differences (Winsock2 vs POSIX), connected streams, client connects, listener ports, and socket-level receive timeouts.
- `vhttp::http` owns HTTP syntax, request parsing, message framing, request/response wire models, trailers, and serialization.
- `vhttp::router` owns request-target interpretation and method-aware dispatch after transport parsing completes.
- `vhttp::server` owns HTTP connection lifecycle decisions and connects transport, parser, handler, and serializer.
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
8. Response framing headers are serializer-authoritative: either `Content-Length` or `Transfer-Encoding: chunked`, never contradictory handler values.

## Persistent connection invariants

1. HTTP/1.1 persists unless a `Connection: close` token is present.
2. HTTP/1.0 closes unless a `Connection: keep-alive` token is present.
3. A response-level close request, configured request ceiling, parse failure, peer EOF, or idle timeout terminates the connection.
4. Pipelined bytes are processed before another blocking `recv()` call.
5. Responses remain ordered because the current runtime dispatches one request at a time.
6. The serializer emits the final connection decision exactly once; application handlers cannot contradict a server-enforced close.

## Verification boundary

`Server::serve_connection()` accepts an already-connected `TcpStream`. Production `listen_and_serve()` uses it after `accept()`, while integration tests create a real loopback listener/client pair and exercise the same connection implementation. Milestone 4 additionally sends a real chunked POST with trailers followed by a pipelined GET, then verifies a chunked response and correct second-request dispatch.

## Planned evolution

1. ✅ Baseline socket + parser + response serializer.
2. ✅ Router and method/request semantics.
3. ✅ Persistent connections, pipelined-byte preservation, request ceilings, and idle retirement.
4. ✅ Chunked transfer decoding/encoding and framing hardening.
5. Static files and conditional/range requests.
6. Thread-pool runtime.
7. Linux `epoll` backend.
8. Windows IOCP backend.
9. Fuzzing, sanitizers, stress tests, and reproducible benchmarks.
