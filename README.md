# Loom core

Compatibility-first C++20 extraction of the reusable ChatADHD engine.

Current branch phase: **v0.07.10 source audit + SQLite/EventBus foundation**.

The first invariant is compatibility with ChatADHD's existing SQLite v4 data model. The v4 tables are treated as a compatibility boundary, not as a claim that the ontology is final.

Build:

```bash
cmake -S . -B build -DLOOM_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

SQLite defaults to the system library. `-DLOOM_USE_BUNDLED_SQLITE=ON` switches to `third_party/sqlite3.c` when the amalgamation is present, preserving the single-compile-unit deployment option without forcing that packaging choice now.

See `docs/PORT_AUDIT.md` before porting another module. It records source facts, prompt/source mismatches, and decisions that remain deliberately open.
