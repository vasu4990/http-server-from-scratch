# vhttp — HTTP/1.1 Server From Scratch

A cross-platform C++20 HTTP server built from raw sockets to demonstrate networking, protocol parsing, routing, connection management, message framing, secure static-file semantics, and later concurrency/performance engineering.

> Status: **Milestone 5 implemented**. The blocking HTTP baseline, method-aware router, persistent HTTP/1.x lifecycle, chunked message framing, and document-root-confined static file engine are implemented. This is still an educational server, not a production-ready internet-facing reverse proxy.

## Why this project exists

The important HTTP server layers are implemented in this repository rather than delegated to an HTTP framework. Normal OS/C++ facilities are used, but TCP integration, incremental parsing, routing, framing, connection lifecycle, static-file semantics, and verification remain visible and testable.

## Current capabilities

### Transport and connections

- Windows Winsock2 and POSIX socket abstraction.
- RAII listeners/streams, TCP client connect support, and ephemeral-port discovery for loopback tests.
- HTTP/1.1 persistence by default; HTTP/1.0 keep-alive opt-in.
- Multiple requests per socket and preserved pipelined bytes.
- Configurable max requests per connection and receive idle timeout.

### HTTP parsing and framing

- Incremental HTTP/1.0 + HTTP/1.1 parsing across arbitrary TCP reads.
- Case-insensitive headers and bounded request/header/body parsing.
- `Content-Length` bodies.
- Incremental `Transfer-Encoding: chunked` decoding, bounded extensions/trailers, overflow/body-budget checks, and terminal-byte preservation.
- Rejection of `Transfer-Encoding` + `Content-Length` ambiguity and unsupported coding chains.
- Chunked response encoding and serializer-authoritative message framing.

### Routing and response semantics

- Method-aware route trie with static and `:parameter` segments.
- Query/path-parameter access.
- Correct `404` / `405`, deterministic `Allow`, automatic `OPTIONS`, and `HEAD` fallback.
- Body-forbidden statuses (`1xx`, `204`, `304`) emit no payload framing/body.

### Secure static file engine

- URL-prefix-to-document-root mapping.
- Strict percent decoding and rejection of encoded/raw separators, backslashes, NUL/control bytes, and `.` / `..` traversal.
- Canonical candidate containment checks after symlink/reparse resolution.
- Explicit directory index policy and regular-file-only serving.
- Configurable maximum file size for the current in-memory GET path.
- MIME detection with `application/octet-stream` fallback and `X-Content-Type-Options: nosniff`.
- Weak ETags + `If-None-Match`.
- `Last-Modified` + `If-Modified-Since`.
- Single byte ranges: closed, open-ended, and suffix forms.
- `206 Partial Content`, `416 Range Not Satisfiable`, `Content-Range`, and date-based `If-Range` policy.
- `HEAD` resolves metadata/ranges without reading file payload bytes.

### Verification

- Unit/adversarial tests for parser, framing, response, router, connection policy, and static-file semantics.
- Filesystem tests for traversal, percent-encoding, MIME, validators, ranges, and symlink escapes where the platform permits symlink creation.
- Real loopback TCP tests for persistent/pipelined requests, chunked request/response flows, idle retirement, and static `GET`/`HEAD`/`206`/`304`/`416` behavior.
- CI for GCC, Clang, and MSVC.

## Architecture

```text
TCP bytes
   |
   v
[socket + connection lifecycle]
   |
   v
[incremental HTTP parser + framing]
   |
   v
 HttpRequest
   |
   v
[application dispatcher]
   |                 |
   v                 v
[static files]    [router]
   |                 |
   +--------+--------+
            v
       HttpResponse
            |
            v
[authoritative wire serializer]
            |
      keep-alive / close
```

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), [`docs/SECURITY.md`](docs/SECURITY.md), and [`docs/ROADMAP.md`](docs/ROADMAP.md).

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

Without static files:

```bash
./build/vhttp_hello
```

The first argument is the port:

```bash
./build/vhttp_hello 18080
```

Pass a document root as the second argument to expose it under `/static`:

```bash
./build/vhttp_hello 8080 ./public
```

Windows example:

```powershell
.\build\Release\vhttp_hello.exe 8080 .\public
```

Example requests:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/users/42
curl -i http://127.0.0.1:8080/chunked
curl -i -X POST --data-binary 'hello' http://127.0.0.1:8080/echo
curl -i http://127.0.0.1:8080/static/index.html
curl -I http://127.0.0.1:8080/static/index.html
curl -i -H 'Range: bytes=0-99' http://127.0.0.1:8080/static/index.html
```

## Engineering constraints and limitations

The server does **not** use Boost.Beast, Crow, cpp-httplib, Drogon, Pistache, or another HTTP server framework.

The repository will not claim production readiness or high performance until fuzzing, stress, scalable runtime, filesystem-race hardening, and benchmark evidence exist.

Important current limitations:

- the accept loop is blocking and serial.
- request bodies and static GET payloads are assembled/read in memory under configured limits.
- static serving supports one byte range, not multipart ranges.
- static ETags are weak metadata validators.
- canonicalize-then-open static confinement rejects traversal and stable symlink escapes but is not race-free against hostile concurrent filesystem mutation.
- no zero-copy file transfer, thread pool, `epoll`, or IOCP runtime yet.

## Next milestone

Milestone 6 introduces bounded concurrency: a fixed worker pool and graceful drain/backpressure first, followed by Linux `epoll` and Windows IOCP backends while preserving the verified HTTP semantics.

## Milestone evidence

- [`docs/MILESTONE_1.md`](docs/MILESTONE_1.md)
- [`docs/MILESTONE_2.md`](docs/MILESTONE_2.md)
- [`docs/MILESTONE_3.md`](docs/MILESTONE_3.md)
- [`docs/MILESTONE_4.md`](docs/MILESTONE_4.md)
- [`docs/MILESTONE_5.md`](docs/MILESTONE_5.md)

## License

MIT.
