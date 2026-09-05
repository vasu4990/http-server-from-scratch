# Milestone 6A — Bounded Thread-Pool Runtime

## Status

Implemented and verified on the feature head with Linux GCC, Linux Clang, and Windows MSVC before the final documentation/PR gate.

## Delivered

- `ThreadPoolRuntime` reuses the existing `Server::serve_connection()` path instead of creating a second HTTP implementation.
- Fixed configurable worker count; no detached threads.
- Bounded accepted-connection queue.
- Explicit saturation behavior: newly accepted transports are closed and counted when the pending queue is full.
- Configurable listen backlog and accept polling interval.
- Timed `TcpListener::accept_for()` implemented with cross-platform `select()` so the accept loop can observe stop requests without unsafe cross-thread listener mutation.
- Graceful stop: stop accepting, drain queued connections, allow active connections to finish, then join every worker before `run()` returns.
- Per-connection exception containment: one failed connection does not terminate a worker or the process-level runtime.
- Runtime statistics: accepted, rejected, completed, failed, active, and queued connections.
- Ephemeral-port discovery and `wait_until_listening()` support deterministic runtime integration tests.

## Verification

The dedicated runtime test suite covers:

- two independent HTTP handlers executing concurrently on two workers.
- bounded-queue saturation with one active, one queued, and an excess transport rejected deterministically.
- graceful stop while a handler is active; the response is allowed to finish before the runtime returns.
- post-drain statistics showing no active or queued work.
- rejection of invalid zero-worker and zero-queue configurations.

All previous parser, framing, router, connection, and static-file tests remain in the same CTest suite, so the concurrent runtime addition is verified without weakening earlier milestones.

## Concurrency contract

Transport and parser state remain local to each connection. `ThreadPoolRuntime` may invoke the configured application `Handler` concurrently. If a handler captures mutable shared application state, the application is responsible for synchronizing that state.

## Important limitations

- This runtime is a blocking worker pool, not an event loop. One persistent/slow connection occupies one worker while it is active.
- Graceful drain does not forcibly cancel active connections; completion remains bounded by connection behavior such as the configured receive idle timeout.
- Queue saturation currently closes the excess accepted TCP connection rather than parsing enough HTTP to synthesize a `503` response.
- Linux `epoll` and Windows IOCP are intentionally deferred to Milestone 6B/6C so their state machines can be reviewed and benchmarked separately.
