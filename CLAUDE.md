# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Petting Zoo is a Svelte frontend client for [zoo-keeper-server](https://github.com/crybo-rybo/zoo-keeper-server), a C++ HTTP server for local LLM inference. This repo contains only the frontend — the server is included as a git submodule and runs via Docker.

## Quick Start

```bash
# Initialize submodules (required on first clone)
git submodule update --init --recursive

# Run with Docker Compose (server + frontend)
docker compose up --build

# Frontend: http://localhost:5173
# Server API: http://localhost:8080
```

## Development (Frontend Only)

```bash
# Install dependencies
npm ci

# Start dev server (requires zoo-keeper-server running on localhost:8080)
npm run dev

# Run tests
npx vitest run

# Build for production
npm run build
```

## Architecture

- **Frontend**: Svelte 5 + Vite + TypeScript at the repo root (`src/`)
- **Server**: zoo-keeper-server git submodule, built via `Dockerfile.server`
- **Orchestration**: Docker Compose runs both services together

### Frontend Structure (`src/`)

- `App.svelte` — Root component
- `routes/` — Page components (e.g., `Health.svelte`)
- `lib/api/client.ts` — Typed HTTP client for zoo-keeper-server API
- `lib/api/types.ts` — TypeScript interfaces for API responses

### Server API (zoo-keeper-server)

The frontend communicates with these endpoints via Vite proxy:

- `GET /healthz` — Server health check
- `GET /v1/models` — List available models
- `POST /v1/sessions` — Create a chat session
- `POST /v1/chat/completions` — Chat completion (streaming supported)
- `GET /metrics` — Server metrics

### Vite Proxy

`vite.config.ts` proxies `/healthz`, `/v1/*`, and `/metrics` to the server. The target URL is controlled by the `VITE_API_URL` env var (default: `http://localhost:8080`). Docker Compose sets this to `http://server:8080`.

## Testing

```bash
# Run all tests
npx vitest run

# Watch mode
npx vitest
```

Tests live in `tests/` mirroring the `src/` structure.

## Development Workflow

- This repo is frontend-only. Core inference logic lives in zoo-keeper-server.
- Add features as vertical slices: API types → client function → component → test.
- Keep components focused — one component per route/feature.
