# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Petting Zoo is an application layer built on top of the [`zoo-keeper`](https://github.com/crybo-rybo/zoo-keeper.git) C++ library (included as a git submodule). It provides:
- A Drogon-based C++ API server
- A Svelte + TypeScript single-page web UI
- Local GGUF model registration/loading from the server filesystem
- Synchronous chat with the active loaded model

**Key principle**: Core inference/runtime logic lives in `zoo-keeper`. This repo handles API hosting, UI, app state, and orchestration only.

## Build Commands

```bash
# Initialize submodule (required on first clone)
git submodule update --init --recursive

# Configure and build
cmake -S . -B build
cmake --build build -j

# Configure with tests enabled
cmake -S . -B build -DPETTING_ZOO_BUILD_TESTS=ON
cmake --build build -j

# Run the server
./build/apps/server/petting_zoo_server
# Server listens on http://127.0.0.1:8080
```

CMake automatically runs `npm ci && npm run build` in `apps/web` to produce frontend assets. If npm fails, fallback assets in `apps/web/fallback-dist/` are served.

## Test Commands

```bash
# Run all tests (C++ unit + server smoke + frontend unit)
ctest --test-dir build --output-on-failure

# Shortcut target
cmake --build build --target check

# Frontend tests only (from apps/web/)
npm run test -- --run

# Frontend tests in watch mode (from apps/web/)
npm run test
```

Tests include:
- `cpp_config_sanity`: C++ sanity test linking against `zoo`
- `server_smoke`: Integration test using `curl` against the live server binary
- `web_unit_tests`: Vitest unit tests in `apps/web/`

## Architecture

### C++ Server (`apps/server/src/`)

The server is structured around a shared `RuntimeState` singleton (mutex-protected) that holds the model registry and active `zoo::Agent`. Route registration is split by domain:

- `runtime_state.{hpp,cpp}` — central state: model registry (`unordered_map`), active model ID, `zoo::Agent` instance
- `routes_health.cpp` — `GET /healthz`
- `routes_models.cpp` — `GET /api/models`, `POST /api/models/register`, `POST /api/models/select`
- `routes_chat.cpp` — `POST /api/chat/complete`, `POST /api/chat/reset`
- `routes_deferred.cpp` — stub handlers returning 501 for future endpoints
- `routes_spa.cpp` — SPA fallback (serves `index.html` for unmatched routes)
- `api_parsers.{hpp,cpp}` — JSON request body parsing
- `api_serialization.{hpp,cpp}` — JSON response serialization
- `http_helpers.{hpp,cpp}` — shared HTTP utilities

The web root path is baked in at compile time via the `PETTING_ZOO_WEB_ROOT` definition.

### Dependencies

- **Drogon** (v1.9.12): HTTP framework, fetched via CMake FetchContent if not found system-wide
- **zoo-keeper** (`zoo`, `zoo_backend`): model runtime, always built from the submodule
- **Svelte 5 + Vite + Vitest**: frontend stack, managed via npm in `apps/web/`

### Frontend (`apps/web/src/`)

Single-component SPA (`App.svelte`) that communicates exclusively with the backend REST API. `lib/chat_format.ts` contains formatting utilities (tested with Vitest in `lib/chat_format.test.ts`).

### Deferred Contracts

Future API contracts are preserved but not implemented:
- `docs/api/openapi.future.yaml` — sessions, streaming, KB, prompts, MCP
- `docs/api/ws-events.md` — WebSocket events

## Development Workflow

- **Contract-first**: define/update API contracts in `docs/api/` before implementation
- **Vertical slices**: add features end-to-end (contract → server route → frontend) one at a time
- **`zoo-keeper` blockers**: when upstream is missing an API or has a bug, open a GitHub issue at `crybo-rybo/zoo-keeper` with reproduction context and reference it in commit notes
- Every feature should include tests (unit + integration minimum) and documentation updates when behavior changes
