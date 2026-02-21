# Petting Zoo

Petting Zoo is an application layer for the [`zoo-keeper`](https://github.com/crybo-rybo/zoo-keeper.git) C++ library.

The repository is intentionally reset to an MVP scope:

- Drogon-based C++ API server
- Svelte + TypeScript web UI
- Local model registration/loading from server filesystem
- Synchronous chat with the active loaded model

## MVP Status

Implemented and supported API endpoints:

- `GET /healthz`
- `GET /api/models`
- `POST /api/models/register`
- `POST /api/models/select`
- `POST /api/chat/complete`
- `POST /api/chat/reset`

Deferred contracts are preserved for future reintroduction:

- `docs/api/openapi.future.yaml` (sessions, streaming, KB, prompts, MCP)
- `docs/api/ws-events.md` (marked deferred)

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

Or use:

- `cmake --build build --target check`

## Development Principles

- Keep core inference/runtime behavior in `zoo-keeper`
- Keep frontend-to-backend API boundary strict
- Keep contracts, implementation, and tests in sync
- Add functionality back in vertical slices after MVP is stable

## Related Project

- `zoo-keeper`: https://github.com/crybo-rybo/zoo-keeper.git
