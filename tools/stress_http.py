#!/usr/bin/env python3
"""Dependency-free HTTP/1.1 stress harness for vhttp.

The harness is intentionally conservative: it is designed to produce reproducible
local evidence, not to claim absolute internet-scale performance. It supports
persistent-connection and connection-churn modes, records latency percentiles,
throughput, status codes, failures, and can emit machine-readable JSON.
"""

from __future__ import annotations

import argparse
import collections
import concurrent.futures
import datetime as dt
import http.client
import json
import math
import os
import pathlib
import platform
import statistics
import sys
import threading
import time
from dataclasses import dataclass
from typing import Counter, Iterable


@dataclass
class WorkerResult:
    latencies_ms: list[float]
    status_counts: Counter[int]
    errors: Counter[str]
    successes: int
    failures: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Reproducible HTTP load harness for vhttp")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8081)
    parser.add_argument("--path", default="/bench")
    parser.add_argument("--requests", type=int, default=5000, help="measured request count")
    parser.add_argument("--concurrency", type=int, default=8, help="parallel client workers")
    parser.add_argument("--warmup", type=int, default=200, help="warm-up requests excluded from results")
    parser.add_argument("--timeout", type=float, default=5.0, help="socket timeout in seconds")
    parser.add_argument(
        "--mode",
        choices=("keepalive", "connect"),
        default="keepalive",
        help="reuse one HTTP connection per client worker or reconnect every request",
    )
    parser.add_argument("--expect-status", type=int, default=200)
    parser.add_argument(
        "--max-error-rate",
        type=float,
        default=0.0,
        help="exit non-zero when measured failures / requests exceed this fraction",
    )
    parser.add_argument("--json-out", help="optional path for machine-readable result JSON")
    args = parser.parse_args()

    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if args.requests <= 0:
        parser.error("--requests must be greater than zero")
    if args.concurrency <= 0:
        parser.error("--concurrency must be greater than zero")
    if args.warmup < 0:
        parser.error("--warmup must not be negative")
    if args.timeout <= 0:
        parser.error("--timeout must be greater than zero")
    if not 0.0 <= args.max_error_rate <= 1.0:
        parser.error("--max-error-rate must be between 0 and 1")
    if not args.path.startswith("/"):
        parser.error("--path must begin with '/'")
    return args


def percentile(sorted_values: list[float], fraction: float) -> float | None:
    if not sorted_values:
        return None
    if len(sorted_values) == 1:
        return sorted_values[0]
    index = (len(sorted_values) - 1) * fraction
    lower = math.floor(index)
    upper = math.ceil(index)
    if lower == upper:
        return sorted_values[lower]
    weight = index - lower
    return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight


def distribute(total: int, workers: int) -> list[int]:
    workers = min(total, workers)
    quotient, remainder = divmod(total, workers)
    return [quotient + (1 if index < remainder else 0) for index in range(workers)]


def new_connection(args: argparse.Namespace) -> http.client.HTTPConnection:
    return http.client.HTTPConnection(args.host, args.port, timeout=args.timeout)


def run_worker(
    request_count: int,
    args: argparse.Namespace,
    start_barrier: threading.Barrier | None,
) -> WorkerResult:
    latencies_ms: list[float] = []
    status_counts: Counter[int] = collections.Counter()
    errors: Counter[str] = collections.Counter()
    successes = 0
    failures = 0
    persistent: http.client.HTTPConnection | None = None

    if start_barrier is not None:
        start_barrier.wait()

    try:
        for _ in range(request_count):
            connection: http.client.HTTPConnection | None = None
            started = time.perf_counter_ns()
            try:
                if args.mode == "keepalive":
                    if persistent is None:
                        persistent = new_connection(args)
                    connection = persistent
                    request_headers = {"Connection": "keep-alive", "User-Agent": "vhttp-stress/1"}
                else:
                    connection = new_connection(args)
                    request_headers = {"Connection": "close", "User-Agent": "vhttp-stress/1"}

                connection.request("GET", args.path, headers=request_headers)
                response = connection.getresponse()
                response.read()
                status_counts[response.status] += 1
                if response.status == args.expect_status:
                    successes += 1
                else:
                    failures += 1
                    errors[f"unexpected HTTP status {response.status}"] += 1
            except Exception as exc:  # noqa: BLE001 - benchmark records transport failures by design.
                failures += 1
                errors[f"{type(exc).__name__}: {exc}"] += 1
                if args.mode == "keepalive" and persistent is not None:
                    persistent.close()
                    persistent = None
            finally:
                elapsed_ms = (time.perf_counter_ns() - started) / 1_000_000.0
                latencies_ms.append(elapsed_ms)
                if args.mode == "connect" and connection is not None:
                    connection.close()
    finally:
        if persistent is not None:
            persistent.close()

    return WorkerResult(latencies_ms, status_counts, errors, successes, failures)


def execute_phase(total: int, args: argparse.Namespace, synchronized_start: bool) -> tuple[WorkerResult, float]:
    if total == 0:
        return WorkerResult([], collections.Counter(), collections.Counter(), 0, 0), 0.0

    allocation = distribute(total, args.concurrency)
    barrier = threading.Barrier(len(allocation)) if synchronized_start and len(allocation) > 1 else None
    aggregate = WorkerResult([], collections.Counter(), collections.Counter(), 0, 0)

    started = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(max_workers=len(allocation)) as executor:
        futures = [executor.submit(run_worker, count, args, barrier) for count in allocation]
        for future in concurrent.futures.as_completed(futures):
            result = future.result()
            aggregate.latencies_ms.extend(result.latencies_ms)
            aggregate.status_counts.update(result.status_counts)
            aggregate.errors.update(result.errors)
            aggregate.successes += result.successes
            aggregate.failures += result.failures
    elapsed = time.perf_counter() - started
    return aggregate, elapsed


def latency_summary(values: Iterable[float]) -> dict[str, float | None]:
    ordered = sorted(values)
    if not ordered:
        return {"min_ms": None, "mean_ms": None, "p50_ms": None, "p95_ms": None, "p99_ms": None, "max_ms": None}
    return {
        "min_ms": ordered[0],
        "mean_ms": statistics.fmean(ordered),
        "p50_ms": percentile(ordered, 0.50),
        "p95_ms": percentile(ordered, 0.95),
        "p99_ms": percentile(ordered, 0.99),
        "max_ms": ordered[-1],
    }


def build_report(args: argparse.Namespace, result: WorkerResult, elapsed: float) -> dict[str, object]:
    total = result.successes + result.failures
    failure_rate = result.failures / total if total else 1.0
    throughput = total / elapsed if elapsed > 0 else 0.0
    success_throughput = result.successes / elapsed if elapsed > 0 else 0.0

    return {
        "schema_version": 1,
        "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "command": [sys.executable, *sys.argv],
        "target": {
            "scheme": "http",
            "host": args.host,
            "port": args.port,
            "path": args.path,
            "expected_status": args.expect_status,
        },
        "load": {
            "mode": args.mode,
            "requests": args.requests,
            "concurrency": min(args.concurrency, args.requests),
            "warmup_requests": args.warmup,
            "timeout_seconds": args.timeout,
        },
        "environment": {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "processor": platform.processor(),
            "logical_cpu_count": os.cpu_count(),
        },
        "results": {
            "elapsed_seconds": elapsed,
            "requests_per_second": throughput,
            "successful_requests_per_second": success_throughput,
            "successes": result.successes,
            "failures": result.failures,
            "failure_rate": failure_rate,
            "status_counts": {str(key): value for key, value in sorted(result.status_counts.items())},
            "top_errors": dict(result.errors.most_common(10)),
            "latency": latency_summary(result.latencies_ms),
        },
    }


def print_report(report: dict[str, object]) -> None:
    load = report["load"]
    results = report["results"]
    assert isinstance(load, dict)
    assert isinstance(results, dict)
    latency = results["latency"]
    assert isinstance(latency, dict)

    def fmt(value: object) -> str:
        return "n/a" if value is None else f"{float(value):.3f}"

    print("vhttp stress result")
    print(f"  mode: {load['mode']}")
    print(f"  measured requests: {load['requests']}")
    print(f"  concurrency: {load['concurrency']}")
    print(f"  elapsed: {float(results['elapsed_seconds']):.3f} s")
    print(f"  throughput: {float(results['requests_per_second']):.2f} req/s")
    print(f"  success throughput: {float(results['successful_requests_per_second']):.2f} req/s")
    print(f"  successes / failures: {results['successes']} / {results['failures']}")
    print(f"  failure rate: {float(results['failure_rate']) * 100.0:.3f}%")
    print(
        "  latency ms: "
        f"min={fmt(latency['min_ms'])} "
        f"mean={fmt(latency['mean_ms'])} "
        f"p50={fmt(latency['p50_ms'])} "
        f"p95={fmt(latency['p95_ms'])} "
        f"p99={fmt(latency['p99_ms'])} "
        f"max={fmt(latency['max_ms'])}"
    )
    print(f"  status counts: {results['status_counts']}")
    if results["top_errors"]:
        print(f"  top errors: {results['top_errors']}")


def main() -> int:
    args = parse_args()

    if args.warmup:
        warmup, _ = execute_phase(args.warmup, args, synchronized_start=False)
        if warmup.failures:
            print(
                f"warning: warm-up observed {warmup.failures} failure(s); measured phase will still run",
                file=sys.stderr,
            )

    result, elapsed = execute_phase(args.requests, args, synchronized_start=True)
    report = build_report(args, result, elapsed)
    print_report(report)

    if args.json_out:
        output = pathlib.Path(args.json_out)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"  JSON: {output}")

    results = report["results"]
    assert isinstance(results, dict)
    return 2 if float(results["failure_rate"]) > args.max_error_rate else 0


if __name__ == "__main__":
    raise SystemExit(main())
