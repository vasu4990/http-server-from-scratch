# Contributing

Changes should be small enough to review and should preserve protocol correctness.

Before opening a pull request:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For parser changes, add tests that split requests at unusual byte boundaries and cover malformed input rather than testing only a normal browser request.
