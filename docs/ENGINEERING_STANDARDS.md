# Engineering Standards

This repository follows evidence-first development.

1. Protocol behavior is implemented against explicit HTTP semantics rather than browser-specific assumptions.
2. Network reads are treated as arbitrary byte fragments, never as message boundaries.
3. Security and performance claims require tests or measurements checked into the repository.
4. New protocol features must add normal, boundary, and malformed-input tests.
5. Cross-platform changes must remain buildable with GCC, Clang, and MSVC.
6. Optimizations must follow profiling and preserve correctness tests.
7. Unsupported behavior is rejected or documented rather than silently approximated.
