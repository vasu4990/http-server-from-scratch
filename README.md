# vhttp — HTTP/1.1 Server From Scratch

A cross-platform C++20 HTTP server built from raw sockets to demonstrate networking, protocol parsing, routing, connection management, message framing, secure static-file semantics, bounded concurrency, and later event-driven/performance engineering.

> Status: **Milestone 6A implemented** with a reproducible local stress/performance evidence harness. The verified HTTP core has a bounded fixed thread-pool runtime with queue backpressure and graceful drain. This remains an educational server, not a production-ready internet-facing reverse proxy.

## Why this project exists

The important HTTP server layers are implemented in this repository rather than delegated to an HTTP framework. Normal OS/C++ facilities are used, but TCP integration, incremental parsing, routing, framing, connection lifecycle, static-file semantics, runtime scheduling, and verification remain visible and testable.

## Current capabilities

### Transport and HTTP connection lifecycle

- Windows Winsock2 and POSIX socket abstraction.
- RAII listeners/streams, TCP client connect support, and ephemeral-port discovery.
- HTTP/1.1 persistence by default; HTTP/1.0 keep-alive opt-in.
- Multiple requests per socket and preserved pipelined bytes.
- Configurable max requests per connection and receive idle timeout.
- Timed listener readiness/accept with cross-platform `select()`.

### HTTP parsing and framing

- Incremental HTTP/1.0 + HTTP/1.1 parsing across arbitrary TCP reads.
- Case-insensitive headers and bounded request/header/body parsing.
- `Content-Length` bodies.
- Incremental `Transfer-Encoding: chunked` decoding with bounded extensions/trailers and overflow/body-budget checks.
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
- MIME detection + `X-Content-Type-Options: nosniff`.
- Weak ETags / `If-None-Match` and `Last-Modified` / `If-Modified-Since`.
- Single closed/open-ended/suffix byte ranges with `206`, `416`, `Content-Range`, and date-based `If-Range` policy.
- `HEAD` resolves metadata/ranges without reading file payload bytes.

### Bounded thread-pool runtime

- Configurable fixed worker count.
- Bounded pending accepted-connection queue.
- Queue saturation closes and counts excess accepted transports instead of growing memory without bound.
- No detached worker/connection threads.
- `request_stop()` stops new acceptance and drains queued + active work before workers are joined.
- Per-connection exception containment.
- Runtime counters for accepted, rejected, completed, failed, active, and queued connections.
- The same `Server::serve_connection()` HTTP implementation is reused by every worker.

### Stress/performance evidence harness

- `vhttp_bench_server` exercises the real thread-pool/parser/router/serializer path with configurable workers, pending capacity, fixed run duration, and response payload size.
- `tools/stress_http.py` uses only the Python standard library.
- Persistent-connection and connection-churn load modes.
- Configurable request count, concurrency, warm-up, timeout, expected status, and tolerated error rate.
- Throughput plus min/mean/p50/p95/p99/max request latency.
- Success/failure, HTTP status, and top transport-error accounting.
- Machine-readable JSON output with client command and environment metadata.
- Explicit benchmark methodology in [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md); no fabricated throughput/latency numbers are committed.

### Verification

- Unit/adversarial tests for parser, framing, responses, routing, connection policy, static files, and thread-pool configuration.
- Filesystem tests for traversal, percent-encoding, MIME, validators, ranges, and symlink escapes where the platform permits symlink creation.
- Real loopback TCP tests for persistent/pipelined requests, chunked flows, static `GET`/`HEAD`/`206`/`304`/`416`, concurrent handler overlap, queue saturation, and graceful active-request drain.
- CI for GCC, Clang, and MSVC.
- CI compiles the benchmark server and syntax-checks the dependency-free load harness; performance numbers themselves are not treated as stable CI assertions.

## Architecture

```text
TcpListener
   |
   +--> serial Server::listen_and_serve()
   |
   `--> bounded ThreadPoolRuntime
              |
        fixed worker threads
              |
              v
      Server::serve_connection()
              |
              v
 [parser/framing -> dispatcher -> serializer]
```

The thread pool changes scheduling, not HTTP semantics. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), [`docs/SECURITY.md`](docs/SECURITY.md), [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md), and [`docs/ROADMAP.md`](docs/ROADMAP.md).

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

## Run the existing example

```bash
./build/vhttp_hello 8080 ./public
```

The first argument is the port. The optional second argument is a document root exposed under `/static`.

Windows:

```powershell
.\build\Release\vhttp_hello.exe 8080 .\public
```

Example requests:

```bash
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/chunked
curl -i -X POST --data-binary 'hello' http://127.0.0.1:8080/echo
curl -i http://127.0.0.1:8080/static/index.html
curl -I http://127.0.0.1:8080/static/index.html
curl -i -H 'Range: bytes=0-99' http://127.0.0.1:8080/static/index.html
```

## Thread-pool embedding

`ThreadPoolRuntime` is a separate runtime wrapper around the same handler/connection core:

```cpp
#include "vhttp/server/thread_pool_runtime.hpp"

vhttp::server::ThreadPoolConfig pool;
pool.worker_count = 8;
pool.max_pending_connections = 256;

vhttp::server::ThreadPoolRuntime runtime(handler, {}, pool);
runtime.run("0.0.0.0", 8080);  // blocks until request_stop()
```

A controlling thread can call `request_stop()`. The accept loop observes it through bounded `accept_for()` polling, then queued/active connections are drained and every worker is joined before `run()` returns.

**Handler concurrency contract:** when the thread-pool runtime is used, application handlers may execute concurrently. Mutable state captured by a handler must be synchronized by the application.

## Generate local performance evidence

Start a fixed-duration benchmark server:

```bash
./build/vhttp_bench_server 8081 4 256 60 128
```

Then, in another terminal, run a persistent-connection baseline:

```bash
python3 tools/stress_http.py --port 8081 --requests 10000 --concurrency 8 --warmup 500 --mode keepalive --json-out benchmark-results/local/keepalive-c8.json
```

Or exercise accept/queue pressure with one connection per request:

```bash
python3 tools/stress_http.py --port 8081 --requests 10000 --concurrency 64 --warmup 500 --mode connect --json-out benchmark-results/local/connect-c64.json
```

See [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md) before publishing or comparing results. Local loopback numbers are not universal capacity claims.

## Engineering constraints and limitations

The server does **not** use Boost.Beast, Crow, cpp-httplib, Drogon, Pistache, or another HTTP server framework.

The repository will not claim production readiness or high performance until the event runtimes, fuzz/stress work, filesystem-race hardening, and broader benchmark evidence exist.

Important current limitations:

- the worker-pool runtime is still blocking: one slow/persistent connection occupies one worker.
- queue saturation closes excess accepted transports instead of returning an HTTP `503`.
- graceful drain waits for active handlers/connections rather than forcibly cancelling them.
- request bodies and static GET payloads are assembled/read in memory under configured limits.
- static serving supports one range, weak metadata ETags, and canonicalize-then-open confinement that is not race-free against hostile concurrent local filesystem mutation.
- the first-party Python harness can become the client-side bottleneck at high request rates and is intended primarily for transparent regression/saturation evidence.
- no zero-copy file path, Linux `epoll`, or Windows IOCP backend yet.

## Next milestone

Milestone 6B builds the Linux nonblocking `epoll` runtime with explicit per-connection read/write state, deadlines, output backpressure, and behavior-parity tests against the blocking/thread-pool core. Windows IOCP follows as Milestone 6C. The new benchmark protocol will then be reused to compare those runtimes on identical load shapes.

## Milestone evidence

- [`docs/MILESTONE_1.md`](docs/MILESTONE_1.md)
- [`docs/MILESTONE_2.md`](docs/MILESTONE_2.md)
- [`docs/MILESTONE_3.md`](docs/MILESTONE_3.md)
- [`docs/MILESTONE_4.md`](docs/MILESTONE_4.md)
- [`docs/MILESTONE_5.md`](docs/MILESTONE_5.md)
- [`docs/MILESTONE_6A.md`](docs/MILESTONE_6A.md)

## License

MIT.
