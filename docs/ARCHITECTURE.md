# Architecture

## HTTP core shared by all runtimes

```text
TcpStream
   |
   v
[connection lifecycle]
   |
   v
[incremental parser + Content-Length/chunked framing]
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
[authoritative serializer]
            |
            v
        TcpStream
```

`Server::serve_connection()` remains the single blocking HTTP connection implementation. Milestone 6A does not fork parser/router/static-file semantics; the thread-pool runtime only changes how accepted streams are scheduled onto that core.

## Runtime layer

```text
                TcpListener
                    |
              accept_for(timeout)
                    |
       +------------+-------------+
       |                          |
 queue has capacity           queue full
       |                          |
       v                          v
 bounded TcpStream queue      close + reject++
       |
       +-------+-------+--- ...
               |
        fixed worker threads
         |      |      |
         v      v      v
      serve_connection()
         |      |      |
         +------v------+
                |
        complete / failed stats
```

`accept_for()` uses cross-platform `select()` readiness with a bounded poll interval. This lets `request_stop()` be observed without closing/mutating a listener from another thread while `accept()` is using it.

## Thread-pool lifecycle invariants

1. Worker count and pending-queue capacity are fixed before `run()` starts.
2. The queue never exceeds its configured bound.
3. An accepted stream that cannot be queued is closed immediately and counted as rejected.
4. Workers are joinable threads; there are no detached connection lifetimes.
5. `request_stop()` stops future acceptance after at most one accept-poll interval.
6. Stop transitions workers into drain mode: queued streams are processed, active streams are allowed to finish, then workers exit and are joined.
7. One connection failure increments the failed counter but cannot terminate another worker.
8. Runtime statistics are concurrency-safe snapshots; queue size is read under the queue mutex.
9. HTTP connection state stays local to each worker invocation, while shared application handler state remains the application's synchronization responsibility.

## Existing HTTP invariants

- HTTP/1.1 persistence / HTTP/1.0 keep-alive policy is unchanged.
- Pipelined bytes are processed before another read.
- Request framing remains unambiguous and bounded.
- Static-file traversal/confinement/validator/range semantics are identical whether a connection is served serially or by the worker pool.
- Responses remain ordered within one connection because `serve_connection()` still processes that connection sequentially.

## Verification boundary

The runtime test starts the real listener on an ephemeral loopback port and uses real TCP clients. It verifies true overlap of two handlers, deterministic bounded-queue saturation, and stop/drain while a response is active. All previous CTest targets execute alongside the new runtime tests.

## Planned evolution

1. ✅ Blocking socket + HTTP parser/serializer.
2. ✅ Routing/request semantics.
3. ✅ Persistent HTTP/1.x lifecycle.
4. ✅ Chunked framing hardening.
5. ✅ Secure static-file semantics.
6. ✅ Bounded fixed blocking worker pool.
7. Linux nonblocking `epoll` runtime with per-connection read/write state and deadlines.
8. Windows IOCP runtime with completion/cancellation lifetime rules.
9. Fuzzing, sanitizers, stress tests, and reproducible benchmarks.
