# vhttp — HTTP/1.1 Server From Scratch

A cross-platform C++20 HTTP server being built from raw sockets to learn and demonstrate networking, protocol parsing, connection management, security hardening, concurrency, and performance engineering.

> Status: **Milestone 1 baseline**. This is not yet a production-ready public-facing server.

## Why this project exists

The goal is to implement the important HTTP server layers ourselves rather than wrapping an HTTP framework. Normal OS and C++ facilities are used, but HTTP parsing, response serialization, connection lifecycle, and later routing/concurrency are implemented in this repository.

## Current capabilities

- Windows Winsock2 and POSIX socket abstraction.
- RAII TCP listener and connected stream.
- Incremental HTTP/1.0 + HTTP/1.1 request parsing.
- Correct handling of HTTP request bytes split across arbitrary TCP reads.
- Header parsing with case-insensitive lookup.
- `Content-Length` request bodies.
- Parser limits for request line, header bytes/count, and body size.
- Conflicting `Content-Length` rejection.
- Explicit rejection of unsupported transfer encodings in this milestone.
- HTTP response serialization with generated `Content-Length`.
- Minimal blocking server and hello-world example.
- CTest coverage for fragmented parsing, framing, limits, and serialization.
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

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the evolving design.

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

Then from another terminal:

```bash
curl -v http://127.0.0.1:8080/
```

Expected body:

```text
Hello from vhttp. This response came from a C++20 HTTP server built from sockets.
```

## Engineering constraints

The server implementation does **not** use Boost.Beast, Crow, cpp-httplib, Drogon, Pistache, or another HTTP server framework.

The repository will not claim production readiness or high performance until the relevant correctness, fuzz, stress, and benchmark evidence exists.

## Roadmap

Next milestones add routing, persistent HTTP/1.1 connections, chunked framing, static files, a thread pool, `epoll`, IOCP, fuzzing, sanitizers, and reproducible performance benchmarks.

See [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Security

The current milestone is educational and should not be exposed directly to untrusted internet traffic. See [`docs/SECURITY.md`](docs/SECURITY.md).

## License

MIT.
