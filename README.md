# Loom Core

Compatibility-first C++20 extraction of the reusable ChatADHD engine.

Loom is intentionally starting from the parts that have a concrete compatibility contract: the existing ChatADHD SQLite v4 data model and event semantics. The goal is to move reusable engine behavior behind a small native core without pretending that the current schema is the final ontology.

## Current state

The merged baseline currently contains:

- a C++20 `loom_core` library;
- SQLite-backed compatibility code for the ChatADHD data model;
- message/graph database paths;
- an EventBus foundation;
- compatibility tests;
- a port audit documenting source facts, mismatches and deliberately open decisions.

This is an extraction/compatibility layer, not yet a drop-in replacement for the whole ChatADHD application.

## Build and test

```bash
cmake -S . -B build -DLOOM_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

By default Loom links the system SQLite library. `-DLOOM_USE_BUNDLED_SQLITE=ON` switches to `third_party/sqlite3.c` when an amalgamation is actually present; the repository does not silently pretend that bundled SQLite exists when it does not.

## Layout

```text
include/       public C++ interfaces
src/           implementation
tests/         compatibility and EventBus tests
docs/          port audit and migration notes
third_party/   optional third-party source location
```

Read `docs/PORT_AUDIT.md` before porting another ChatADHD module. It records what was observed in the source, what was inferred, and which design choices remain intentionally unresolved.

## Design constraint

Compatibility is a boundary, not an architecture endorsement. Existing database behavior is preserved first so that later refactors can be tested against something concrete instead of rewriting semantics and implementation at the same time.

## Licensing

This project is **source-available**, not OSI open-source. Noncommercial use is licensed under the PolyForm Noncommercial License 1.0.0; see `LICENSE`.

Commercial use requires a separate written license; see `COMMERCIAL_LICENSE.md`. Third-party code retains its own licensing.
