# vhttp — HTTP/1.1 Server From Scratch

A cross-platform C++20 HTTP server being built from raw sockets to learn and demonstrate networking, protocol parsing, routing, connection management, security hardening, concurrency, and performance engineering.

> Status: **Milestone 2 complete**. The blocking HTTP baseline and method-aware router are implemented and CI-verified. This is not yet a production-ready public-facing server.

## Why this project exists

The goal is to implement the important HTTP server layers ourselves rather than wrapping an HTTP framework. Normal OS and C++ facilities are used, but TCP integration, HTTP parsing, routing, response serialization, connection lifecycle, and later concurrency are implemented in this repository.

## Current capabilities

- Windows Winsock2 and POSIX socket abstraction.
- RAII TCP listener and connected stream.
- Incremental HTTP/1.0 + HTTP/1.1 request parsing.
- Correct handling of HTTP request bytes split across arbitrary TCP reads.
- Header parsing with case-insensitive lookup.
- `Content-Length` request bodies.
- Parser limits for request line, header bytes/count, and body size.
- Conflicting `Content-Length` rejection.
- Explicit rejection of unsupported transfer encodings until chunked framing is implemented.
- HTTP response serialization with generated `Content-Length`.
- Method-aware route trie.
- Static and `:parameter` route segments with deterministic static-route precedence.
- Path parameter and query-string accessors, including repeated query keys.
- Correct `404` versus `405` dispatch behavior with `Allow` headers.
- `HEAD` fallback to `GET` with response-body suppression on the wire.
- Automatic `OPTIONS` responses for registered paths.
- CTest coverage for fragmented parsing, framing, response serialization, and routing semantics.
- CI for GCC, Clang, and MSVC.

## Architecture

```text
TCP bytes
   |
   v
[platform socket layer]
   |
   v
[incremental HTTP parser]
   |
   v
 HttpRequest
   |
   v
[method-aware route trie]
   |
   v
  handler
   |
   v
 HttpResponse
   |
   v
[wire serializer]
   |
   v
TCP bytes
```

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the evolving design and [`docs/ROADMAP.md`](docs/ROADMAP.md) for milestone sequencing.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Windows with Visual Studio:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Run the example

```bash
./build/vhttp_hello
```

The default port is `8080`; pass a different port as the first argument, for example `./build/vhttp_hello 18080`.

Example requests:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/users/42
curl -I http://127.0.0.1:8080/health
curl -i -X OPTIONS http://127.0.0.1:8080/health
```

## Engineering constraints

The server implementation does **not** use Boost.Beast, Crow, cpp-httplib, Drogon, Pistache, or another HTTP server framework.

The repository will not claim production readiness or high performance until the relevant correctness, fuzz, stress, and benchmark evidence exists.

## Next milestone

Milestone 3 focuses on the HTTP/1.1 connection lifecycle: persistent connections, configurable idle limits, multiple requests per socket, preservation of pipelined bytes, and graceful shutdown behavior.

## Security

The current milestone is educational and should not be exposed directly to untrusted internet traffic. See [`docs/SECURITY.md`](docs/SECURITY.md).

## Milestone evidence

- [`docs/MILESTONE_1.md`](docs/MILESTONE_1.md)
- [`docs/MILESTONE_2.md`](docs/MILESTONE_2.md)

## License

MIT.
