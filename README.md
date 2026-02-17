# Petting Zoo

Petting Zoo is an application layer for the [`zoo-keeper`](https://github.com/crybo-rybo/zoo-keeper.git) C++ library. It provides a local app experience with:

- A Drogon-based C++ API server
- A Svelte + TypeScript web UI
- Streaming chat over WebSocket
- Session, model, prompt, memory, and knowledge-base management
- MCP connector support for external tools/resources

## Status
Implemented through an MCP management + minimal runtime interaction slice:
- `apps/server`: Drogon host with `/healthz` and SPA/static serving
- `apps/web`: Svelte + TypeScript + Vite frontend scaffold
- CMake build integration to produce and serve web assets
- MCP connector API endpoints:
  - `GET /api/mcp/catalog`
  - `GET/POST /api/mcp/connectors`
  - `POST /api/mcp/connectors/validate`
  - `POST /api/mcp/connectors/{id}/connect`
  - `POST /api/mcp/connectors/{id}/disconnect`
  - `POST /api/mcp/connectors/{id}/refresh-tools`
  - `GET /api/mcp/connectors/{id}/tools`
- MCP connector management UI (template/custom create, validate, connect/disconnect, tool list)
- Model runtime endpoints:
  - `GET /api/models`
  - `POST /api/models/register`
  - `POST /api/models/select`
  - `POST /api/chat/complete`
  - `POST /api/chat/reset`
- Web UI sections for:
  - model registration/loading from server filesystem
  - prompt/response interaction with active loaded model

See `APPLICATION_PLAN.md` for the full phased roadmap.

## Phase 0 Artifacts
- REST API contract: `docs/api/openapi.yaml`
- WebSocket stream contract: `docs/api/ws-events.md`
- Error envelope and taxonomy: `docs/api/errors.md`
- Runtime config template: `config/app.example.yaml`

## Submodule Setup
- Initialize dependencies after clone:
  - `git submodule update --init --recursive`

## Quickstart
1. Configure and build:
   - `cmake -S . -B build`
   - `cmake --build build -j`
2. Run the server:
   - `./build/apps/server/petting_zoo_server`
3. Open:
   - `http://127.0.0.1:8080`

Notes:
- By default, CMake attempts `npm ci && npm run build` in `apps/web` to produce assets.
- If npm build fails, fallback static assets in `apps/web/fallback-dist/` are served.

## Testing
Run all project tests (C++ + server smoke + frontend unit tests) through CTest:

1. Configure with tests enabled:
   - `cmake -S . -B build -DPETTING_ZOO_BUILD_TESTS=ON`
2. Build:
   - `cmake --build build -j`
3. Run:
   - `ctest --test-dir build --output-on-failure`

You can also use the unified target:
- `cmake --build build --target check`

CI runs this test flow automatically on every push and pull request via `.github/workflows/ci.yml`.

## Goals
- Keep core inference logic in `zoo-keeper`
- Provide a clean API boundary between UI and runtime
- Ship incrementally with testable milestones
- Support local-first workflows and robust observability

## Planned Stack
- Backend: C++ + Drogon
- Frontend: Svelte + TypeScript (Vite)
- Persistence: SQLite + `zoo-keeper` context DB
- Protocols: REST + WebSocket + MCP (JSON-RPC 2.0)

## Planned Repository Structure
- `apps/server/` API host
- `apps/web/` UI app
- `include/zoo_app/` app interfaces
- `src/zoo_app/` app implementations
- `config/` runtime config
- `docs/api/` API and stream contracts

## Primary Milestones
1. Contracts and skeleton
2. Core model/session/chat runtime
3. Knowledge + memory workflows
4. Prompt management
5. MCP connectors
6. Hardening and packaging

## Development Principles
- API contract-first development (OpenAPI + stream schema)
- Vertical slices over broad incomplete scaffolds
- Strict definition of done: tests, telemetry, docs, and UX parity

## Related Project
- `zoo-keeper`: https://github.com/crybo-rybo/zoo-keeper.git
