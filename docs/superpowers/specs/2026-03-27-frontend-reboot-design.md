# Petting Zoo Frontend Reboot — Design Spec

## Context

Petting Zoo currently bundles a C++ Drogon server, a zoo-keeper submodule, and a Svelte frontend into a single build. The zoo-keeper-server project (https://github.com/crybo-rybo/zoo-keeper-server) now provides a standalone HTTP server with the same inference capabilities plus sessions, streaming, tools, and metrics. This redesign removes all server-side code from petting-zoo and repositions it as a pure frontend client for zoo-keeper-server.

The initial milestone is minimal: a single health-check dashboard page that proves the new architecture works end-to-end.

## Decisions

- **Fresh Svelte project** — no code carried over from the old frontend
- **zoo-keeper-server as git submodule** — version-pinned, built from source in Docker
- **Docker Compose** — orchestrates server and frontend dev server together
- **Auto-polling health page** — polls `/healthz` every 5 seconds with live status indicator

## Project Structure

```
petting-zoo/
├── zoo-keeper-server/          # Git submodule
├── docker-compose.yml          # server + frontend services
├── Dockerfile.server           # Multi-stage build for zoo-keeper-server
├── package.json                # Svelte + Vite + TypeScript
├── vite.config.ts              # Dev server, proxies /healthz and /v1/* to server
├── tsconfig.json
├── svelte.config.js
├── src/
│   ├── main.ts                 # Mounts App.svelte
│   ├── app.css                 # Global styles
│   ├── App.svelte              # Root — renders Health component
│   ├── lib/
│   │   └── api/
│   │       ├── client.ts       # fetchHealth() — typed fetch wrapper
│   │       └── types.ts        # HealthResponse interface
│   └── routes/
│       └── Health.svelte       # Health dashboard component
├── public/                     # Static assets (favicon)
├── tests/
│   └── lib/
│       └── api/
│           └── client.test.ts  # Unit tests for API client
├── .gitignore
├── CLAUDE.md
└── README.md
```

## What Gets Deleted

All of these are removed from the repo:

- `apps/` — old server and old frontend
- `cmake/` — C++ build utilities
- `config/` — server config files
- `docs/` — old API docs (this spec is written before deletion)
- `tests/` — old C++ and integration tests
- `zoo-keeper/` — old submodule (replaced by zoo-keeper-server)
- `CMakeLists.txt` — C++ root build
- `AGENTS.md`, `RECOMMENDATIONS.md` — no longer applicable
- `uploads/` — server-side storage

Preserved: `.git/`, `.github/`, `.gitignore` (updated), `CLAUDE.md` (rewritten), `README.md` (rewritten).

## Docker Setup

### Dockerfile.server

Multi-stage build:
1. **Build stage**: Base image with CMake + C++ toolchain. Copies `zoo-keeper-server/` submodule, runs `git submodule update --init --recursive`, then `cmake -S . -B build && cmake --build build --parallel`.
2. **Runtime stage**: Minimal image. Copies built binary and a default config. Exposes port 8080.

### docker-compose.yml

```yaml
services:
  server:
    build:
      context: .
      dockerfile: Dockerfile.server
    ports:
      - "8080:8080"
    volumes:
      - ./models:/models  # Mount local GGUF models
    # Config will need a model_path pointing to /models/<file>.gguf

  frontend:
    image: node:22-alpine
    working_dir: /app
    command: sh -c "npm ci && npm run dev -- --host"
    ports:
      - "5173:5173"
    volumes:
      - .:/app
      - node_modules:/app/node_modules
    depends_on:
      - server

volumes:
  node_modules:
```

### Vite Proxy

```typescript
// vite.config.ts
export default defineConfig({
  server: {
    proxy: {
      '/healthz': 'http://server:8080',
      '/v1': 'http://server:8080',
      '/metrics': 'http://server:8080',
    }
  }
})
```

The proxy target is controlled by the `VITE_API_URL` env var, defaulting to `http://localhost:8080` for native dev. The Docker Compose frontend service sets `VITE_API_URL=http://server:8080` so the container-to-container networking works.

## API Client

### types.ts

```typescript
export interface HealthResponse {
  status: string;
  ready: boolean;
  version: string;
  model_id: string;
}
```

### client.ts

```typescript
export async function fetchHealth(): Promise<HealthResponse> {
  const res = await fetch('/healthz');
  if (!res.ok) throw new Error(`Health check failed: ${res.status}`);
  return res.json();
}
```

## Health.svelte Component

Displays:
- **Status indicator**: Green circle when `ready === true`, red when false or unreachable
- **Status text**: "Ready", "Starting", or "Unreachable"
- **Server details** (when connected): version, model_id, status
- **Last checked timestamp**

Behavior:
- Calls `fetchHealth()` on mount
- Sets up `setInterval` at 5 seconds for auto-polling
- Clears interval on component destroy
- Catches fetch errors and displays "Server unreachable" state

Styling: Minimal CSS, no framework. Dark background, monospace font for data fields, colored status dot.

## Testing

### Unit Tests (Vitest)

- `client.test.ts`: Tests `fetchHealth()` with mocked fetch — success case, HTTP error, network error
- Run with `npm run test`

### Manual Integration Test

1. `docker compose up`
2. Open `http://localhost:5173`
3. Verify green status indicator and health data displayed
4. Stop server container, verify red indicator and "Unreachable" message
5. Restart server, verify recovery

## Updated CLAUDE.md

Will be rewritten to reflect:
- New architecture (pure frontend client for zoo-keeper-server)
- Build commands (`npm ci`, `npm run dev`, `docker compose up`)
- Test commands (`npm run test`)
- Project structure description
- Development workflow (frontend-only, server via Docker)

## Updated README.md

Will cover:
- What the project is (frontend for zoo-keeper-server)
- Prerequisites (Docker, Node 22+)
- Quick start (`docker compose up`)
- Development setup
- Project structure

## Verification

1. `docker compose up --build` starts both services without errors
2. `curl http://localhost:8080/healthz` returns valid JSON from the server
3. `http://localhost:5173` shows the health dashboard with live data
4. Stopping the server container causes the UI to show "Unreachable"
5. `npm run test` passes all unit tests
6. No old files remain (no `apps/`, `cmake/`, `CMakeLists.txt`, etc.)
