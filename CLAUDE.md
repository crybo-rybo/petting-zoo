# CLAUDE.md

<constraints>
- NEVER modify files inside `zoo-keeper-server/` — it is a read-only git submodule
- NEVER commit `.env` files, model files (`.gguf`), or `node_modules/`
- ALWAYS run `./scripts/test` and confirm all tests pass before committing
- ALWAYS use Svelte 5 runes (`$state`, `$derived`, `$effect`), not legacy stores
- ALWAYS co-locate tests next to source: `foo.test.ts` beside `foo.ts`
</constraints>

## WHAT

Petting Zoo is a Svelte 5 frontend client for zoo-keeper-server, a C++ HTTP server
for local LLM inference. This repo is **frontend-only**. The server is a git submodule
built and run via Docker.

See `AGENTS.md` for the full cross-tool agent reference (commands, project structure,
API contract, boundaries). See `spec.md` for the living project specification.

## WHY

The frontend is decoupled from the inference engine. zoo-keeper-server is complex C++
with its own CMake build system; petting-zoo is a lightweight Svelte app that talks to
it over HTTP. This separation lets each evolve independently.

## HOW

### Quick Start

```bash
# First-time setup
./scripts/bootstrap

# Full stack via Docker (server + frontend)
docker compose up --build
# Frontend: http://localhost:5173  |  Server API: http://localhost:8080

# Frontend-only development (requires zoo-keeper-server on localhost:8080)
npm run dev
```

### Verification Commands

```bash
./scripts/test        # Run all unit tests
./scripts/typecheck   # TypeScript + Svelte type checking
npm run build         # Production build
```

### Adding a Feature

Follow the vertical slice pattern:

1. Define types in `src/lib/api/types.ts` (or a new feature directory)
2. Write the API client function with a co-located test
3. Build the Svelte component in `src/routes/`
4. Verify: `./scripts/test && ./scripts/typecheck`

### Architecture

- **Frontend**: Svelte 5 + Vite + TypeScript at repo root (`src/`)
- **Server**: zoo-keeper-server git submodule, built via `Dockerfile.server`
- **Orchestration**: Docker Compose runs both services together

### Frontend Structure

```
src/
  main.ts              — App entry point, mounts App.svelte
  App.svelte           — Root component
  app.css              — Global styles (dark theme)
  lib/api/
    client.ts          — Typed HTTP client (fetchHealth, etc.)
    client.test.ts     — Co-located unit tests
    types.ts           — API response interfaces (HealthResponse, etc.)
  routes/
    Health.svelte      — Health dashboard with 5s auto-polling
```

### Server API Endpoints

| Method | Path                   | Purpose                     |
|--------|------------------------|-----------------------------|
| GET    | `/healthz`             | Server health check         |
| GET    | `/v1/models`           | List available models       |
| POST   | `/v1/sessions`         | Create a chat session       |
| POST   | `/v1/chat/completions` | Chat completion (streaming) |
| GET    | `/metrics`             | Prometheus metrics          |

### Vite Proxy

`vite.config.ts` proxies `/healthz`, `/v1/*`, and `/metrics` to the server.
Target URL is `VITE_API_URL` (default: `http://localhost:8080`).
Docker Compose sets this to `http://server:8080`.

### Testing

- Framework: Vitest 3 with mocked `fetch` and dynamic `await import()` for module reset
- Tests are co-located with source files (`*.test.ts` next to the code they test)
- Run a single test: `npx vitest run src/path/to/file.test.ts`
