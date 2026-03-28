# spec.md — Petting Zoo

## Objective

Web frontend for local LLM inference via zoo-keeper-server. Provides a browser-based
UI for monitoring server health, managing chat sessions, and interacting with
locally-hosted language models.

## Tech Stack

| Tool       | Version | Purpose                  |
|------------|---------|--------------------------|
| Svelte     | 5.x     | UI framework (runes API) |
| Vite       | 6.x     | Build tool / dev server  |
| TypeScript | 5.6     | Type safety (strict)     |
| Vitest     | 3.x     | Unit testing             |
| Docker     | —       | Server containerization  |
| Node.js    | 22      | Runtime (dev + Docker)   |

## Data Models

### HealthResponse
```typescript
interface HealthResponse {
  status: string    // e.g. "ready", "starting"
  ready: boolean    // true when model is loaded and accepting requests
  version: string   // server version, e.g. "0.0.4"
  model_id: string  // loaded model identifier, e.g. "local-model"
}
```

## API Contracts

All endpoints are served by zoo-keeper-server. The frontend accesses them
via Vite's dev proxy (see `vite.config.ts`).

| Method | Path                   | Request Body          | Response          | Notes              |
|--------|------------------------|-----------------------|-------------------|--------------------|
| GET    | `/healthz`             | —                     | `HealthResponse`  | Polled every 5s    |
| GET    | `/v1/models`           | —                     | Model list        | Not yet implemented |
| POST   | `/v1/sessions`         | Session config        | Session object    | Not yet implemented |
| POST   | `/v1/chat/completions` | Messages + parameters | SSE stream / JSON | Not yet implemented |
| GET    | `/metrics`             | —                     | Prometheus text   | Not yet implemented |

## Testing Strategy

- **Unit tests:** Vitest with mocked `fetch`, co-located with source (`*.test.ts`)
- **Type checking:** `svelte-check` with strict TypeScript
- **Integration:** Manual via `docker compose up --build`
- **CI:** GitHub Actions runs tests, type check, and build on every push

## Features

- [x] Health dashboard — auto-polling status indicator with details table
- [ ] Model listing — display available models from `/v1/models`
- [ ] Chat interface — streaming chat via `/v1/chat/completions`
- [ ] Metrics dashboard — server metrics from `/metrics`

## Boundaries

- This repo is frontend-only. No server-side code.
- `zoo-keeper-server/` is a read-only git submodule.
- No runtime dependencies beyond Svelte — no UI framework library, no state management library.
- Model files (`.gguf`) are never committed; they are volume-mounted via Docker.
