# Milestone 6A.1 — Stress and Performance Evidence Harness

## Status

Implementation milestone for generating repeatable local load evidence around the bounded blocking runtime.

This milestone adds measurement infrastructure. It does **not** claim a particular throughput, latency, or production capacity because no machine-specific benchmark result should be presented as universal evidence.

## Delivered

- `vhttp_bench_server`, compiled with the normal examples, exercises the real `ThreadPoolRuntime` plus parser/router/response serialization path.
- Configurable benchmark-server TCP port, worker count, bounded pending queue, fixed run duration, and response payload size.
- Fixed-duration execution followed by graceful `request_stop()` drain and final runtime counters.
- `tools/stress_http.py`, implemented using only the Python standard library.
- `keepalive` mode for persistent-connection load.
- `connect` mode for one-TCP-connection-per-request accept/queue pressure.
- Configurable measured request count, client concurrency, warm-up requests, timeout, expected status, and allowed error rate.
- Measured elapsed time, total/successful requests per second, success/failure counts, status distribution, top errors, and request-latency min/mean/p50/p95/p99/max.
- Optional JSON evidence containing the exact client command, timestamp, load shape, basic client-machine metadata, and measured results.
- Local raw benchmark output path excluded from normal source-control noise.
- `docs/PERFORMANCE.md` defines the benchmark matrix, repeated-run policy, reporting requirements, and interpretation limits.
- Linux CI syntax-checks the Python harness; normal GCC/Clang/MSVC builds compile the benchmark server because it is part of the examples build.

## Evidence rules

Curated results should be published only when they are actually measured. A publishable result must identify at least:

- server Git revision,
- Release build,
- compiler and version,
- operating system,
- CPU/logical cores and RAM,
- server worker count and pending-queue capacity,
- payload size,
- client mode, request count, concurrency, warm-up, and timeout,
- whether the client and server ran on the same machine,
- repeated runs rather than a single favorable sample.

The first-party Python client can itself become the bottleneck at high rates, so it is suitable for transparent regression/saturation evidence but not sufficient for universal peak-throughput claims.

## Relationship to later milestones

M6B (`epoll`) and M6C (IOCP) should reuse this protocol so runtime comparisons use the same routes, payloads, client modes, concurrency levels, and reporting rules.

M7 remains responsible for deeper verification and performance work such as fuzzing, sanitizers, malformed-request corpora, external high-rate load tools, CPU/RSS profiling, and curated cross-revision benchmark evidence.
