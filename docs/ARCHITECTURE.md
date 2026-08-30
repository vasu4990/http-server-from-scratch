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
                        |
                        v
                    HttpRequest
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

The runtime intentionally remains blocking and accepts one connection at a time. This keeps HTTP state-machine and lifecycle correctness observable before thread pools or event-driven I/O are introduced.

## Design boundaries

- `vhttp::net` owns OS socket differences (Winsock2 vs POSIX), connected streams, client connects, listener ports, and socket-level receive timeouts.
- `vhttp::http` owns HTTP syntax, request parsing, request/response wire models, and serialization.
- `vhttp::router` owns request-target interpretation and method-aware dispatch after transport parsing completes.
- `vhttp::server` owns HTTP connection lifecycle decisions and connects transport, parser, handler, and serializer.
- The parser is incremental: TCP packet boundaries are never treated as HTTP message boundaries.
- Parsed bytes beyond one complete request remain owned by the parser until the server explicitly moves them into the next parser cycle.

## Persistent connection invariants

1. HTTP/1.1 persists unless a `Connection: close` token is present.
2. HTTP/1.0 closes unless a `Connection: keep-alive` token is present.
3. A response-level close request, configured request ceiling, parse failure, peer EOF, or idle timeout terminates the connection.
4. Pipelined bytes are processed before another blocking `recv()` call.
5. Responses remain ordered because the current runtime dispatches one request at a time.
6. The serializer emits the final connection decision exactly once; application handlers cannot contradict a server-enforced close.

## Verification boundary

`Server::serve_connection()` accepts an already-connected `TcpStream`. Production `listen_and_serve()` uses it after `accept()`, while integration tests create a real loopback listener/client pair and exercise the same connection implementation. No mock transport path is required for persistent-connection tests.

## Planned evolution

1. ✅ Baseline socket + parser + response serializer.
2. ✅ Router and method/request semantics.
3. ✅ Persistent connections, pipelined-byte preservation, request ceilings, and idle retirement.
4. Chunked transfer decoding/encoding and framing hardening.
5. Static files and conditional/range requests.
6. Thread-pool runtime.
7. Linux `epoll` backend.
8. Windows IOCP backend.
9. Fuzzing, sanitizers, stress tests, and reproducible benchmarks.
