# Milestone 3 Plan — HTTP/1.1 Connection Lifecycle

## Objective

Move from one-request-per-connection behavior to explicit HTTP/1.1 persistent connection management without conflating transport boundaries with message boundaries.

## Work packages

1. Connection policy configuration: idle timeout, maximum requests per connection, and read limits.
2. Keep-alive decision logic for HTTP/1.1 and HTTP/1.0 semantics.
3. Parser reuse/reset while preserving bytes already read beyond the completed request.
4. Multiple sequential requests on one TCP connection.
5. Pipelined request preservation and ordered response handling in the blocking runtime.
6. Correct `Connection: close` behavior and deterministic shutdown.
7. Integration tests using raw sockets rather than only `curl`.
8. Adversarial tests for premature EOF, partial next requests, and close directives.

## Exit gates

- Multiple requests over one socket pass on Linux and Windows CI.
- Pipelined bytes survive parser resets without loss or duplication.
- HTTP/1.0 and HTTP/1.1 connection-close rules have dedicated tests.
- No claim of asynchronous concurrency is made in this milestone.
