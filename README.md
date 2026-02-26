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
- `GET /api/chat/stream`
- `POST /api/chat/reset`
- `POST /api/chat/clear_memory`
- `GET /api/mcp/connectors`

Deferred contracts are preserved for future reintroduction:

- `docs/api/openapi.future.yaml` (sessions, KB, prompts)
- `docs/api/ws-events.md` (marked deferred)

## Configuration

The server is configured via `config/app.json`.

- **Model Loading**: For security against path traversal, models can only be registered if their absolute path falls strictly within one of the directories specified in `runtime.model_discovery_paths`.
- **MCP Connectors**: For security against arbitrary remote code execution, MCP connectors are strictly configured via the `mcp_connectors` array. Dynamic registration via the API is disabled.
- **Port Override**: You can override the native server port configured in `server.port` by setting the `PORT` environment variable (e.g., `PORT=9090 ./build/apps/server/petting_zoo_server`).

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
