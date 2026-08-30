# vhttp — HTTP/1.1 Server From Scratch

A cross-platform C++20 HTTP server being built from raw sockets to learn and demonstrate networking, protocol parsing, routing, connection management, security hardening, concurrency, and performance engineering.

> Status: **Milestone 4 complete**. The blocking HTTP baseline, method-aware router, persistent HTTP/1.x connections, and secure chunked message framing are implemented and CI-verified. This is not yet a production-ready public-facing server.

## Why this project exists

The goal is to implement the important HTTP server layers ourselves rather than wrapping an HTTP framework. Normal OS and C++ facilities are used, but TCP integration, HTTP parsing, routing, message framing, response serialization, connection lifecycle, and later concurrency are implemented in this repository.

## Current capabilities

### Transport and connection lifecycle

- Windows Winsock2 and POSIX socket abstraction.
- RAII TCP listener and connected stream.
- TCP client connect support plus ephemeral bound-port discovery for loopback verification.
- Cross-platform socket receive idle timeouts.
- HTTP/1.1 persistent connections by default.
- HTTP/1.0 explicit `keep-alive` opt-in behavior.
- Multiple requests per accepted TCP connection.
- Pipelined next-request byte preservation across parser resets.
- Configurable maximum requests per connection.
- Configurable idle timeout for silent/partial persistent connections.

### HTTP parsing and framing

- Incremental HTTP/1.0 + HTTP/1.1 request parsing across arbitrary TCP reads.
- Header parsing with case-insensitive lookup.
- `Content-Length` request bodies.
- `Transfer-Encoding: chunked` request decoding.
- Hexadecimal chunk sizes with overflow and body-budget validation.
- Bounded chunk extension lines.
- Terminal zero chunks and bounded request trailers.
- Case-insensitive trailer lookup.
- Preservation of bytes after chunked trailers for the next pipelined request.
- Rejection of `Transfer-Encoding` + `Content-Length` ambiguity.
- Rejection of unsupported transfer-coding chains and HTTP/1.0 chunked requests.
- Parser limits for request line, headers, body, chunk line, trailer bytes, and trailer count.
- Conflicting `Content-Length` rejection.

### Responses and routing

- Serializer-authoritative `Content-Length` and `Connection` headers.
- Optional chunked response encoding with terminal zero chunk.
- Method-aware route trie.
- Static and `:parameter` route segments with deterministic static-route precedence.
- Path parameter and query-string accessors, including repeated query keys.
- Correct `404` versus `405` dispatch behavior with `Allow` headers.
- `HEAD` fallback to `GET` with response-body suppression on the wire.
- Automatic `OPTIONS` responses for registered paths.

### Verification

- Unit/adversarial tests for fragmented parsing, Content-Length framing, chunked framing, response serialization, routing, and connection semantics.
- Real loopback TCP tests for persistent connections, pipelining, max-request closure, idle retirement, chunked POST + trailers, chunked responses, and a following pipelined request.
- CI for GCC, Clang, and MSVC.

## Architecture

```text
TCP bytes
   |
   v
[platform socket layer]
   |
   v
[connection lifecycle + idle/request limits]
   |
   v
[incremental HTTP parser]
   |        |
   |        +--> Content-Length
   |        `--> chunk-size -> data -> CRLF -> trailers
   v
 HttpRequest (decoded body + trailers)
   |
   v
[HTTP/1.x persistence policy]
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
   +--> Content-Length framing
   `--> chunked framing
   |
   v
[authoritative wire serializer]
   |
   +---- keep alive ----> preserved next request bytes / recv
   |
   `---- close ---------> deterministic socket retirement
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
curl -i http://127.0.0.1:8080/chunked
curl -i -X POST --data-binary 'hello from request body' http://127.0.0.1:8080/echo
```

The test suite contains raw TCP examples for true chunked requests and trailers; the `/chunked` example endpoint emits a chunked response.

## Engineering constraints

The server implementation does **not** use Boost.Beast, Crow, cpp-httplib, Drogon, Pistache, or another HTTP server framework.

The repository will not claim production readiness or high performance until the relevant correctness, fuzz, stress, and benchmark evidence exists.

The current accept loop remains intentionally blocking and serial. Request bodies are assembled in memory under configured limits, and the current chunked response encoder emits an in-memory body as one data chunk. Scalable concurrent and streaming runtimes are later milestones.

## Next milestone

Milestone 5 builds the static file engine: canonical path normalization, document-root confinement, MIME detection, ETags/conditional requests, modification dates, byte ranges, `HEAD` parity, and traversal/symlink hardening.

## Security

The current milestone is educational and should not be exposed directly to untrusted internet traffic. See [`docs/SECURITY.md`](docs/SECURITY.md).

## Milestone evidence

- [`docs/MILESTONE_1.md`](docs/MILESTONE_1.md)
- [`docs/MILESTONE_2.md`](docs/MILESTONE_2.md)
- [`docs/MILESTONE_3.md`](docs/MILESTONE_3.md)
- [`docs/MILESTONE_4.md`](docs/MILESTONE_4.md)

## License

MIT.
