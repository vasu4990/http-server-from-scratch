# Performance and Stress Evidence

This document defines how performance evidence for `vhttp` should be collected and reported.

The repository does **not** claim production readiness or universal throughput numbers. Results from this harness are local measurements that are meaningful only when the server revision, build, machine, operating system, compiler, runtime configuration, load shape, and client methodology are recorded.

## What is included

Two first-party tools provide a repeatable baseline without third-party benchmark dependencies:

- `vhttp_bench_server`: a small benchmark target backed by the real `ThreadPoolRuntime` and the same parser/router/serializer path used by the library.
- `tools/stress_http.py`: a Python-standard-library load generator that records throughput, successes/failures, HTTP status counts, transport errors, and latency min/mean/p50/p95/p99/max. It can write the raw result as JSON.

This harness is intended to answer questions such as:

- Does throughput scale when the worker count changes?
- At what offered concurrency does latency start rising sharply?
- Does the bounded queue reject/close excess transports under connection churn instead of growing without bound?
- Are persistent connections materially different from one-connection-per-request load?
- Does a code change regress throughput, latency, or error rate on the same machine and load profile?

It is **not** a substitute for later `wrk`/external-host testing, CPU/RSS profiling, sanitizer/fuzz work, or event-runtime benchmarks.

## Build

Release builds should be used for performance measurements.

Linux/macOS-style CMake invocation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Windows with Visual Studio:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

## Start the benchmark server

Linux/macOS-style build:

```bash
./build/vhttp_bench_server 8081 4 256 60 128
```

Windows:

```powershell
.\build\Release\vhttp_bench_server.exe 8081 4 256 60 128
```

Arguments, in order:

1. TCP port.
2. fixed worker count.
3. maximum pending accepted connections.
4. server lifetime in seconds.
5. response payload size in bytes, capped at 1 MiB.

The benchmark endpoint is `GET /bench`. The server prints its configuration at startup and the runtime accepted/rejected/completed/failed/active/queued counters after the fixed-duration run drains.

A fixed server lifetime makes individual experiments self-terminating and reduces the temptation to leave an untracked background process running.

## Run the load client

### Persistent-connection baseline

```bash
python3 tools/stress_http.py \
  --host 127.0.0.1 \
  --port 8081 \
  --path /bench \
  --requests 10000 \
  --concurrency 8 \
  --warmup 500 \
  --mode keepalive \
  --json-out benchmark-results/local/keepalive-c8.json
```

### Connection-churn / accept-path load

```bash
python3 tools/stress_http.py \
  --host 127.0.0.1 \
  --port 8081 \
  --path /bench \
  --requests 10000 \
  --concurrency 64 \
  --warmup 500 \
  --mode connect \
  --json-out benchmark-results/local/connect-c64.json
```

`keepalive` gives each client worker a persistent `HTTPConnection`; `connect` creates a fresh TCP connection for every measured request. The latter includes connection setup in request latency and places much more pressure on the accept/pending-queue path.

By default the harness exits non-zero if any measured request fails or returns an unexpected status. For an intentional saturation experiment, raise `--max-error-rate` explicitly and report that choice with the results.

## Required benchmark matrix

A useful local baseline should vary one dimension at a time.

### Worker scaling

Hold payload, queue capacity, request count, and client concurrency constant. Run worker counts such as:

```text
1, 2, 4, 8
```

Do not assume more workers are better. Compare throughput and p95/p99 latency.

### Offered concurrency

For one worker configuration, test client concurrency around and above the worker count, for example:

```text
workers x 1
workers x 2
workers x 4
workers x 8
```

Run both `keepalive` and `connect` when investigating scheduler/accept behavior.

### Queue saturation

Use a deliberately small pending queue and connection-churn mode. Record both client-side failures and the benchmark server's final `rejected` counter. A saturation result is useful only when the worker count, pending capacity, client concurrency, and error-rate policy are all shown.

### Payload sensitivity

Use the benchmark server's payload argument to compare small and moderate response bodies without changing application logic, for example:

```text
128 B
4 KiB
64 KiB
```

Large payload tests increasingly measure memory copies, socket buffering, and the client harness rather than only parser/router scheduling.

## Repetition and reporting rules

For a publishable result:

1. Build Release from a clean, identified Git commit.
2. Record CPU, logical core count, RAM, OS version, compiler/version, and whether the client runs on the same machine.
3. Keep power mode and major background workload reasonably stable.
4. Run a warm-up before every measured phase.
5. Repeat the same configuration at least five times.
6. Keep every raw JSON result.
7. Report the median requests/second across runs plus representative p50/p95/p99 latency and failure rate.
8. Never combine numbers from different machines as though they were directly comparable.
9. Never label local-loopback results as internet-facing capacity.
10. Preserve failures and saturation behavior; do not delete inconvenient runs without explaining why they were excluded.

## JSON evidence

`--json-out` records:

- timestamp and exact client command,
- target endpoint and expected status,
- mode, request count, concurrency, warm-up, and timeout,
- Python/platform/CPU-count metadata,
- elapsed time and requests/second,
- successful requests/second,
- successes, failures, and failure rate,
- HTTP status counts and top transport/status errors,
- min/mean/p50/p95/p99/max latency.

The client does not know the server Git SHA, compiler, server worker count, queue capacity, or machine RAM. Those must accompany curated benchmark evidence separately.

## Interpretation limits

The first-party Python harness prioritizes transparency and portability over maximum load-generation capacity. At high request rates, Python scheduling, the GIL, loopback networking, client CPU, or `http.client` may become the bottleneck before `vhttp` does.

Therefore:

- use it for repeatable regression and saturation evidence now;
- use external-host `wrk`/similar load later for higher-rate throughput characterization;
- profile server CPU/RSS before attributing a plateau to the HTTP implementation;
- compare the future `epoll` and IOCP runtimes under the same request/payload matrix rather than using unrelated headline numbers.

## Current evidence boundary

The repository now contains the mechanism to generate stress/performance evidence. It intentionally does **not** commit invented throughput or latency figures. Curated benchmark results should be added only after they are actually measured on a named revision under this protocol.
