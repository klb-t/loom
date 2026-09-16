# ChatADHD v0.07.10 → Loom source audit

Status: source audit completed before C++ implementation.

The `klb-t/chatadhd` `main` branch was read file-by-file before this bootstrap was written. The latest visible commit is newer than the v0.07.10 feature commit but `main.py` still identifies the application as v0.07.10.

## Python files reviewed

`core/__init__.py`, `core/crypto.py`, `core/selector.py`, `core/semantic.py`;
`engine/__init__.py`, `engine/batch_api.py`, `engine/chat_engine.py`, `engine/config.py`, `engine/db.py`, `engine/events.py`, `engine/github_sync.py`, `engine/graph_engine.py`, `engine/graph_memory.py`, `engine/importer.py`, `engine/memory_engine.py`, `engine/models.py`, `engine/paths.py`, `engine/providers.py`, `engine/semantic_llm.py`, `engine/semantic_worker.py`;
`gui/__init__.py`, `gui/base.py`, `gui/chat_panel.py`, `gui/conv_panel.py`, `gui/dialogs.py`, `gui/github_panel.py`, `gui/graph_viz.py`, `gui/import_panel.py`, `gui/memory_panel.py`, `gui/voice_panel.py`, and `main.py`.

## Confirmed source behavior

### Database
- SQLite schema version 4.
- WAL + `synchronous=NORMAL` + foreign keys.
- `conversations`, `messages`, `nodes`, `links`, `_meta`.
- Message version groups: editing creates a new row and marks the previous active version `version`.
- `semantic_status` drives the background semantic queue.
- Batch insert uses chunks of 1000.
- Graph links are application-level upserts on `(src,dst,link_type)`.

### Semantic path
- `core/semantic.py` is regex/keyword/heuristic analysis only.
- `engine/semantic_llm.py` performs optional LLM enrichment with regex fallback and disables LLM analysis after 5 consecutive failures.
- The semantic worker has regex, one-by-one LLM, and Anthropic batch paths.

### Context / memory
There are three distinct mechanisms and they must not be accidentally collapsed during the port:
1. hierarchical user memory (`memory_engine.py`),
2. graph-proximity context (`graph_memory.py`),
3. generic 3-tier search (`core/selector.py`: embedding → TF-IDF → keyword).

### Import
Entry points exist for ZIP, SQLite, JSON, JSONL/NDJSON, HTML, MHT/MHTML, screenshots, Markdown and text. Large top-level JSON arrays use a character-level streaming parser.

### Providers / integrations
The Python source contains more than the old proposed Loom module list implies: OpenRouter chat/model registry, direct Anthropic batch path, Groq and Google ASR provider classes, OCR.space, GitHub sync, attachment processing, and web/deep-research request flags. None is considered rejected merely because the old C++ module list omitted it.

## Prompt ↔ source mismatches / open conflicts

These are deliberately not resolved by pretending one side never existed.

### OPEN-01 — Context ranking
The historical C++ prompt proposes `relevance × recency × weight` after graph expansion. Actual `graph_memory.py` does seed extraction → exact node lookup → BFS → message collection, with traversal/depth/node-count limits and no explicit formula. The ranking formula is therefore an extension/proposal, not an exact port. Retrieval policy stays replaceable.

### OPEN-02 — “Preserve all functionality” vs module list
The old core list omits or under-specifies hierarchical memory internals, model registry details, ASR/OCR providers and GitHub sync, while the proposed C ABI does include memory calls. Omission is not rejection.

### OPEN-03 — Voice
The provider layer contains ASR clients, but `gui/voice_panel.py` recording is explicitly a placeholder (`not yet implemented`). “Voice input” is partial infrastructure, not a fully working end-to-end v0.07.10 feature.

### OPEN-04 — ChatGPT SQLite import
The format is advertised, but `_import_chatgpt_db()` is an empty stub. ChatGPT JSON mapping import is implemented. Porting must preserve what actually exists without pretending the SQLite handler was complete.

### OPEN-05 — ChatGPT branch import
The JSON importer comment says it walks the active path, but implementation recursively walks every child branch and flattens messages into the target DB without preserving source branch parentage. Whether Loom reproduces that flattening or preserves the source tree behind a compatibility mode remains OPEN.

### OPEN-06 — EventBus thread safety
Python `EventBus` is synchronous and has no lock. The historical C++ prompt explicitly requires thread safety. C++ keeps synchronous delivery but protects the registry and invokes callbacks outside the lock; this satisfies the newer constraint without changing data compatibility.

### OPEN-07 — Relation/type representation
`core/semantic.py` uses small Python enums, while later architecture treats relation definitions as registry data. Do not hard-code that enum set as final Loom ontology. v4 already stores `links.link_type` as text, giving a compatibility bridge.

### OPEN-08 — UI stack
The old prompt proposes Android JNI + WebView/React and desktop Tauri/FFI. Later work considered other UI stacks. Core implementation does not need to decide this; the C ABI remains the platform-neutral boundary.

### OPEN-09 — Crypto provider
Python uses `cryptography` for AES-256-GCM with PBKDF2-SHA256/600k. The old C++ prompt proposes OpenSSL. Wire/algorithm compatibility matters more than choosing the provider now, so crypto waits for compatibility vectors.

## Source defects / edge cases to turn into tests

These are observations, not automatic permission to change user-visible behavior:
- `ChatEngine.send()` persists the current user message before `_build_messages()` reads conversation history, then appends the current user message again; this appears to duplicate the current turn in the API payload.
- `GraphEngine` contains a silent exception path despite the project convention that errors should be logged.
- `count_pending_semantic()` counts all pending rows while worker selection filters active messages with length >=20, so displayed pending work can diverge from drainable work.
- semantic batch logic exists in two places with differing key/config assumptions.
- ZIP import calls `extractall()` without path-traversal validation.
- JSONL import loads all non-empty lines before processing despite the general streaming goal.
- import completion can be emitted by both the inner message import path and outer `import_file()` path.
- GitHub sync compares file sizes rather than content hashes.
- model/provider presets and UI provider colors are hard-coded in GUI code.
- graph-visualization limits are UI constraints and must not leak into core graph storage/query semantics.

## First implementation tranche

Implemented first because it does not force unresolved product choices:
1. exact v4-compatible SQLite tables/indexes,
2. forward repair/migration for legacy tables,
3. conversation/message/version CRUD,
4. node/link CRUD,
5. pending semantic queue primitives,
6. thread-safe synchronous EventBus,
7. tests for fresh DB, legacy migration, versioning, graph links and re-entrant event dispatch.

The full historical `loom.h` target ABI is checked in as a contract, but symbols beyond implemented modules are intentionally not faked. `loom_c_api.cpp` remains a later step, matching the original implementation order.
