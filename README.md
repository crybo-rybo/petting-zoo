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

- CMake builds frontend assets into `build/web-dist` (the directory served by the backend).
- Frontend packaging uses `npm ci` (when dependencies change) and `npm run build`.
- By default, frontend build failures stop the build. To allow fallback assets, configure with:
  - `cmake -S . -B build -DPETTING_ZOO_WEB_ALLOW_FALLBACK=ON`

## Frontend Development Loop

For iterative UI work, run Vite separately from the packaged static build:

1. Start backend API server (terminal A):
   - `cmake --build build -j --target petting_zoo_server`
   - `./build/apps/server/petting_zoo_server`
2. Start Vite dev server (terminal B):
   - `npm --prefix apps/web run dev`
3. Open:
   - `http://127.0.0.1:5173`

Notes:

- Vite proxies `/api` and `/healthz` to `http://127.0.0.1:8080`.
- This dev loop avoids stale packaged assets while editing frontend code.

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
