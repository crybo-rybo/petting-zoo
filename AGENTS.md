# AGENTS.md

## Project

Petting Zoo — Svelte 5 frontend client for [zoo-keeper-server](https://github.com/crybo-rybo/zoo-keeper-server),
a C++ HTTP server for local LLM inference. This repo is **frontend-only**; the server
is included as a git submodule and runs via Docker.

## Tech Stack

| Tool       | Version | Purpose                  |
|------------|---------|--------------------------|
| Svelte     | 5.x     | UI framework (runes API) |
| Vite       | 6.x     | Build tool / dev server  |
| TypeScript | 5.6     | Type safety (strict)     |
| Vitest     | 3.x     | Unit testing             |
| Docker     | —       | Server containerization  |

## Commands

- Bootstrap: `./scripts/bootstrap`
- Test: `./scripts/test`
- Type check: `./scripts/typecheck`
- Dev server: `npm run dev` (requires zoo-keeper-server on localhost:8080)
- Production build: `npm run build`
- Full stack (Docker): `docker compose up --build`

## Project Structure

```
src/
  main.ts              — App entry point
  app.css              — Global styles (dark theme)
  App.svelte           — Root component
  vite-env.d.ts        — Vite type declarations
  lib/api/
    client.ts          — Typed HTTP client (fetchHealth, etc.)
    client.test.ts     — Co-located unit tests
    types.ts           — API response interfaces
  routes/
    Health.svelte      — Health dashboard with 5s auto-polling
```

## Code Style

- TypeScript strict mode, no `any`
- Svelte 5 runes (`$state`, `$derived`, `$effect`) — not legacy stores
- Co-locate tests: `foo.ts` tested by `foo.test.ts` in the same directory
- Descriptive names: `fetchHealth()` not `getH()`
- One primary export per file
- Files under 300 lines, functions under 30 lines

## Testing

- Framework: Vitest 3
- Pattern: mock `fetch` globally with `vi.stubGlobal`, use dynamic `await import()` to reset modules between tests
- Run single file: `npx vitest run src/path/to/file.test.ts`
- All tests must pass before committing

## API Contract (zoo-keeper-server)

| Method | Path                   | Purpose                    | Response Type    |
|--------|------------------------|----------------------------|------------------|
| GET    | `/healthz`             | Server health check        | `HealthResponse` |
| GET    | `/v1/models`           | List available models      | —                |
| POST   | `/v1/sessions`         | Create a chat session      | —                |
| POST   | `/v1/chat/completions` | Chat completion (streaming) | —               |
| GET    | `/metrics`             | Prometheus metrics         | —                |

Vite proxies all API routes to `VITE_API_URL` (default: `http://localhost:8080`).
Docker Compose sets this to `http://server:8080`.

## Git Workflow

- Feature branches off `main`
- Conventional commits: `feat:`, `fix:`, `docs:`, `chore:`
- Tests and type check must pass before committing

<AgentBoundaries>
## Boundaries

### Always (no permission needed)
- Read any file, run tests, run type check, run build

### Ask first
- Adding new npm dependencies
- Modifying Docker or CI configuration
- Changing the Vite proxy configuration

### Never
- Modify files inside `zoo-keeper-server/` (read-only git submodule)
- Commit `.env` files, model files (`.gguf`), or `node_modules/`
- Add server-side code — this is a frontend-only repo
</AgentBoundaries>
