# Architecture

## Milestone 1 baseline

```text
Client
  |
  v
TcpListener --accept--> TcpStream
                        |
                        v
                 HttpRequestParser
                        |
                        v
                    HttpRequest
                        |
                        v
                      Handler
                        |
                        v
                    HttpResponse
                        |
                        v
                 Wire serializer
                        |
                        v
                    TcpStream
```

The first milestone intentionally uses a blocking, one-connection-at-a-time execution model. That makes protocol correctness observable before concurrency is introduced.

## Design boundaries

- `vhttp::net` owns OS socket differences (Winsock2 vs POSIX).
- `vhttp::http` owns HTTP syntax and wire models.
- `vhttp::server` connects transport and HTTP without hiding either layer behind a framework.
- The parser is incremental: TCP packet boundaries are never treated as HTTP message boundaries.

## Planned evolution

1. Baseline socket + parser + response serializer.
2. Router and middleware boundaries.
3. Persistent connections and request pipelining safety.
4. Chunked transfer decoding.
5. Static files and conditional/range requests.
6. Thread-pool runtime.
7. Linux `epoll` backend.
8. Windows IOCP backend.
9. Fuzzing, sanitizers, stress tests, and reproducible benchmarks.
