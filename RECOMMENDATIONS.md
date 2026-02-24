# Petting Zoo — Production Readiness Audit

Comprehensive evaluation of the codebase, prioritized from critical issues that must be resolved before any production exposure, down to cleanup items.

---

## 1. Should Definitely Change (Blockers)

### 1.1 Path Traversal — Arbitrary File Read
**`apps/server/src/routes_spa.cpp:18-20`**

The SPA catch-all route joins the raw URL path to `web_root` without canonicalization. A request to `/../../../etc/passwd` traverses out of the web root and serves any file readable by the server process. Must validate that the resolved canonical path starts with the web root prefix.

### 1.2 Remote Command Execution via MCP Connector Registration
**`apps/server/src/routes_mcp.cpp:12-28` + `runtime_state.cpp:261-278`**

`POST /api/mcp/connectors` accepts an arbitrary `command` and `args` from the client, stored and passed to `zoo::mcp::McpClient` for subprocess execution. There is zero validation — no allowlist, no path restriction. Combined with the lack of CORS (1.4), any browser tab on the same machine can execute arbitrary commands as the server user.

### 1.3 Filesystem Probing via Model Registration
**`apps/server/src/runtime_state.cpp:58-63` + `api_serialization.cpp:8`**

`POST /api/models/register` accepts any filesystem path, probes its existence, and reflects the full absolute path back in the JSON response. This enables file-existence enumeration and leaks internal filesystem structure. Restrict to a configurable model directory allowlist.

### 1.4 No CORS Headers
**`apps/server/src/main.cpp`**

No CORS policy is set anywhere. The server binds to `127.0.0.1`, but any website the user visits can silently make requests to `localhost:8080` — registering models, running MCP commands, chatting with the model. Add strict `Access-Control-Allow-Origin` for the served origin only.

### 1.5 No Application Logging
**Entire `apps/server/src/`**

Zero application-level log output. Model loads, chat requests, errors, MCP command executions — none produce any log. The only logging is Drogon's framework-level output at `kWarn`. This makes debugging, incident response, and auditing impossible.

### 1.6 Configuration Is Entirely Hardcoded
**`apps/server/src/main.cpp:26-27`, `runtime_state.hpp:22`**

`config/app.example.yaml` exists and is well-structured but completely unused. Bind address (`127.0.0.1`), port (`8080`), context size (`8192`), log level, and web root are all hardcoded or compile-time only. Implement runtime config loading from this YAML file (or environment variables at minimum).

### 1.7 SSE JSON Parse Has No Error Handling
**`apps/web/src/features/chat/stream.ts:48`**

`JSON.parse(part.slice(6))` has no try/catch. A single malformed SSE payload from the server crashes the entire stream consumer, losing the partial response accumulated so far. Wrap in try/catch and surface the error gracefully.

---

## 2. Should Change (High Priority)

### 2.1 Fragile Dual-Mutex Locking Discipline
**`apps/server/src/runtime_state.cpp` — throughout**

`RuntimeState` uses `mu_` and `agent_mu_` with inconsistent acquisition patterns. `unload_model()` and `clear_memory()` hold `mu_` while acquiring `agent_mu_`, but `chat_complete()` / `chat_stream()` release `mu_` before acquiring `agent_mu_`, creating a TOCTOU window where the agent can be swapped between the two lock acquisitions. The overall locking contract is fragile — one future change acquiring locks in the wrong order introduces deadlock. Consider a single mutex, or document and enforce the lock ordering invariant with assertions.

### 2.2 Detached Threads with No Shutdown Coordination
**`apps/server/src/routes_chat.cpp:77-135`**

Streaming responses spawn `std::thread(...).detach()`. On process exit (SIGTERM, Ctrl-C), detached threads running inference are forcefully killed mid-operation. No signal handler, no cancellation token, no join. This risks model state corruption and unclean resource release. Use joinable threads tracked by the runtime, with a shutdown signal.

### 2.3 No Binary Hardening Flags
**`apps/server/CMakeLists.txt:31-35`**

Warnings are enabled (`-Wall -Wextra -Wpedantic`), but no security hardening: no `-D_FORTIFY_SOURCE=2`, `-fstack-protector-strong`, `-fPIE`/`-pie`, or `-Wl,-z,relro,-z,now`. Add these for release builds.

### 2.4 `.gitignore` Gaps — Runtime Data at Risk of Commit
**`.gitignore`**

`uploads/` (root and `apps/web/uploads/`) contain `memory.db` SQLite databases and 256-entry `tmp/` hash directories. These are untracked but not gitignored — one `git add -A` away from being committed. Also missing: `*.db`, `*.sqlite`, `*.gguf`, `.env`, `.cache/`, `*.log`. `test_memory.py` (committed) contains a hardcoded user-specific model path.

### 2.5 Zero C++ Unit Tests for Application Code
**`tests/`**

The only C++ test (`config_sanity`) tests the upstream `zoo` library, not petting-zoo code. Route handlers, API parsers (`api_parsers.cpp`), serialization (`api_serialization.cpp`), and `RuntimeState` logic are entirely untested at the unit level. The smoke test covers happy-path HTTP but not edge cases, concurrency, or error paths.

### 2.6 Frontend Test Coverage Gaps
**`apps/web/src/`**

No tests for: `shared/api/client.ts` (request/error logic), `features/models/service.ts`, `features/mcp/service.ts`, or any Svelte component. `App.svelte` (1097 lines) and `McpPanel.svelte` (458 lines) have zero tests. Existing tests miss edge cases (malformed SSE, empty streams, zero-value metrics).

### 2.7 Smoke Test Hardcodes Port 8080
**`tests/integration/server_smoke.sh:12`**

If port 8080 is already in use (dev server running, another test), the smoke test either fails or tests the wrong server. Accept a port via environment variable or find a free port dynamically.

### 2.8 Deferred Routes Return 404 Instead of 501
**`apps/server/src/routes_deferred.cpp:11`**

Unimplemented-but-contracted endpoints return `404 Not Found`. RFC 7231 specifies `501 Not Implemented` for this case. 404 makes it impossible for clients to distinguish "endpoint doesn't exist" from "endpoint exists but isn't ready yet."

---

## 3. Should Consider (Medium Priority)

### 3.1 `App.svelte` Is a 1097-Line Monolith
**`apps/web/src/App.svelte`**

Handles model management, chat, auto-scroll, error display, and ~700 lines of CSS in one file. Extract `ChatMessage`, `ChatInput`, `ModelModal`, and `ErrorBanner` components for readability and testability.

### 3.2 Accessibility Deficiencies
**`apps/web/src/App.svelte:267-307`, `McpPanel.svelte:145-222`**

Modals lack: focus traps, Escape-to-close, `role="dialog"`, `aria-modal="true"`. Buttons suppress a11y linter warnings (`svelte-ignore`) rather than fixing them. No `aria-live` region for new chat messages. Close button has no accessible label.

### 3.3 Stop Button Creates Phantom Message
**`apps/web/src/App.svelte:184-186`**

Clicking "Stop" during streaming appends a second assistant message with `*(Generation stopped)*` while the partial message remains. User sees two consecutive assistant bubbles.

### 3.4 Empty Assistant Bubble on Stream Failure
**`apps/web/src/App.svelte:158`**

An empty `content: ''` assistant message is pushed before streaming starts. If the stream fails before any tokens arrive, the user sees an empty bubble with just the "Zoo Model" header.

### 3.5 No Input Length Limits (Client or Server)
**`apps/web/src/App.svelte` (textarea) + `apps/server/src/api_parsers.cpp:57-77`**

Neither the textarea nor the server parser enforce a maximum message length. Extremely large inputs can cause memory pressure, context overflow, or prolonged inference blocking `agent_mu_`.

### 3.6 CI Pipeline Is Minimal
**`.github/workflows/ci.yml`**

Single platform (ubuntu-latest), no caching (CMake, npm, FetchContent), no linting (clang-tidy, eslint, tsc), no artifact upload, hardcoded `-j 2`. Builds are likely slow. No macOS coverage despite CMakeLists supporting it.

### 3.7 OpenAPI Spec Is Out of Sync with Implementation
**`docs/api/openapi.yaml` vs server source**

Undocumented endpoints: `POST /api/models/unload`, `POST /api/chat/clear_memory`, `POST /api/chat/stream`, `GET/POST/DELETE /api/mcp/*`. Error category `"server_error"` used in code but absent from the spec enum. Version mismatch: CMake says `0.1.0`, OpenAPI says `0.2.0`.

### 3.8 No Rate Limiting or Request Size Limits
**Entire server**

No rate limiting on any endpoint. Inference endpoints (`/api/chat/complete`, `/api/chat/stream`) are CPU/GPU-intensive and can be trivially abused for resource exhaustion, especially with no auth and no CORS.

### 3.9 Unused and Mismatched npm Dependencies
**`apps/web/package.json`**

- `marked-highlight` is listed but never imported (unused dependency).
- `@types/marked` (`^5.0.2`) conflicts with `marked` v17 which bundles its own types.
- `@types/highlight.js` (`^9.12.4`) is deprecated; `highlight.js` v11 bundles its own types.

---

## 4. Cleanup

### 4.1 Remove `test_memory.py`
Committed ad-hoc script with hardcoded user-specific path (`/Users/conorrybacki/.models/...`). Not integrated into test suite. Remove or move to a `scripts/` directory with parameterization.

### 4.2 Remove Stale Documentation References
`AGENTS.md` references nonexistent `APPLICATION_PLAN.md` and directories (`include/zoo_app/`, `src/zoo_app/`). Update or remove.

### 4.3 Update CLAUDE.md Frontend Description
States "Single-component SPA (App.svelte)" — now also includes `McpPanel.svelte` and a `features/`+`shared/` service layer.

### 4.4 Database Uses Relative Path
**`apps/server/src/runtime_state.cpp:28`** — `"uploads/memory.db"` resolves relative to CWD. If the server is started from an unexpected directory, the database appears in the wrong location. Use an absolute path derived from config or the executable's location.

### 4.5 Empty `.vscode/settings.json`
Contains only `{}`. Either add useful workspace settings or remove and gitignore the directory.

### 4.6 Drogon FetchContent Not Hash-Pinned
**`cmake/FetchDependencies.cmake:17-22`** — Uses `GIT_TAG v1.9.12` without a commit SHA. If the tag is ever force-pushed, the build silently pulls different code. Pin to the specific commit hash.

### 4.7 Unnecessary SQLite in Drogon Build
**`cmake/FetchDependencies.cmake:16`** — `BUILD_SQLITE ON` is set for Drogon, but the server uses zoo-keeper's SQLite, not Drogon's ORM. Adds build time and binary size for no benefit.

### 4.8 `ChatMessage` Type Defined Inline
**`apps/web/src/App.svelte:53`** — Should live in `shared/api/types.ts` alongside other shared types.

### 4.9 `token: any` in Markdown Renderer
**`apps/web/src/lib/markdown_render.ts:14,27`** — Loses type safety. Use `marked.Tokens.Code` and `marked.Tokens.HTML` respectively.

### 4.10 Correlation ID Not Length-Bounded
**`apps/server/src/http_helpers.cpp:42-48`** — Client-supplied `X-Correlation-Id` is reflected verbatim with no length limit. Cap at a reasonable length (e.g., 128 chars).
