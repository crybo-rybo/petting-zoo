# Frontend Reboot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the C++ server + Svelte monolith with a pure Svelte frontend client backed by zoo-keeper-server via Docker Compose, starting with a health-check dashboard.

**Architecture:** Fresh Svelte 5 + Vite + TypeScript project at the repo root. zoo-keeper-server added as a git submodule, built from source in a multi-stage Dockerfile. Docker Compose orchestrates both services. Vite proxies API calls to the server.

**Tech Stack:** Svelte 5, Vite 6, TypeScript 5, Vitest, Docker, Docker Compose

**Spec:** `docs/superpowers/specs/2026-03-27-frontend-reboot-design.md`

---

### Task 1: Remove old code and submodule

**Files:**
- Delete: `apps/`, `cmake/`, `config/`, `docs/`, `tests/`, `uploads/`, `CMakeLists.txt`, `AGENTS.md`, `RECOMMENDATIONS.md`
- Modify: `.gitmodules` (remove zoo-keeper entry)

- [ ] **Step 1: Deinit and remove the zoo-keeper submodule**

```bash
git submodule deinit -f zoo-keeper
git rm -f zoo-keeper
rm -rf .git/modules/zoo-keeper
```

- [ ] **Step 2: Delete all old directories and files**

```bash
git rm -rf apps/ cmake/ config/ docs/ tests/ uploads/
git rm -f CMakeLists.txt AGENTS.md RECOMMENDATIONS.md
```

- [ ] **Step 3: Clean up .gitmodules**

If `.gitmodules` is now empty, remove it:

```bash
git rm -f .gitmodules
```

- [ ] **Step 4: Commit the cleanup**

```bash
git add -A
git commit -m "chore: remove old C++ server, zoo-keeper submodule, and all legacy code

Preparing for frontend-only architecture backed by zoo-keeper-server."
```

---

### Task 2: Add zoo-keeper-server submodule

**Files:**
- Create: `.gitmodules` (new entry for zoo-keeper-server)
- Create: `zoo-keeper-server/` (submodule checkout)

- [ ] **Step 1: Add the submodule**

```bash
git submodule add https://github.com/crybo-rybo/zoo-keeper-server.git zoo-keeper-server
```

- [ ] **Step 2: Initialize recursive submodules**

zoo-keeper-server has its own submodules (zoo-keeper library):

```bash
cd zoo-keeper-server && git submodule update --init --recursive && cd ..
```

- [ ] **Step 3: Commit**

```bash
git add .gitmodules zoo-keeper-server
git commit -m "chore: add zoo-keeper-server as git submodule"
```

---

### Task 3: Create Dockerfile.server

**Files:**
- Create: `Dockerfile.server`
- Create: `config/server.json` (default server config for Docker)

- [ ] **Step 1: Create the server config**

Create `config/server.json`:

```json
{
  "bind": "0.0.0.0:8080",
  "model_id": "local-model",
  "api_key": null,
  "http": {
    "max_body_size": 1048576,
    "memory_body_limit": 65536,
    "idle_timeout": 60
  },
  "session": {
    "max_concurrent": 1,
    "inactive_timeout": 900
  },
  "zoo": {
    "model_path": "/models/model.gguf",
    "context_size": 2048,
    "gpu_layers": 0,
    "system_message": "You are a helpful assistant.",
    "sampling": {
      "temperature": 0.7,
      "top_p": 0.9,
      "top_k": 40,
      "repeat_penalty": 1.1
    },
    "mmap": true,
    "mlock": false,
    "max_message_history": 64,
    "request_queue_capacity": 64
  },
  "tools": []
}
```

Note: `bind` is `0.0.0.0:8080` (not `127.0.0.1`) so the container accepts connections from Docker networking. `model_path` points to `/models/model.gguf` which will be volume-mounted.

- [ ] **Step 2: Create Dockerfile.server**

Create `Dockerfile.server`:

```dockerfile
# --- Build stage ---
FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates uuid-dev libjsoncpp-dev \
    libssl-dev zlib1g-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY zoo-keeper-server/ .

RUN git submodule update --init --recursive && \
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel

# --- Runtime stage ---
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libjsoncpp25 libssl3t64 libuuid1 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/zoo_keeper_server /usr/local/bin/zoo_keeper_server
COPY config/server.json /etc/zoo-keeper-server/server.json

EXPOSE 8080

ENTRYPOINT ["zoo_keeper_server", "/etc/zoo-keeper-server/server.json"]
```

- [ ] **Step 3: Verify Dockerfile syntax**

```bash
docker build --check -f Dockerfile.server .
```

If `--check` is not supported on the local Docker version, skip this step — the build will be validated during the Docker Compose task.

- [ ] **Step 4: Commit**

```bash
git add Dockerfile.server config/server.json
git commit -m "feat: add Dockerfile.server and default server config

Multi-stage build for zoo-keeper-server. Binds 0.0.0.0:8080,
model path at /models/model.gguf (volume-mounted)."
```

---

### Task 4: Scaffold Svelte project

**Files:**
- Create: `package.json`, `vite.config.ts`, `tsconfig.json`, `tsconfig.node.json`, `svelte.config.js`, `index.html`, `src/main.ts`, `src/app.css`, `src/App.svelte`, `src/vite-env.d.ts`, `public/` (empty dir)

- [ ] **Step 1: Initialize the Svelte project**

Run the Svelte scaffolding tool from the repo root. Since we're adding to an existing git repo, we create in a temp dir and move files:

```bash
npm create vite@latest petting-zoo-temp -- --template svelte-ts
```

- [ ] **Step 2: Move scaffolded files to repo root**

```bash
cp petting-zoo-temp/package.json .
cp petting-zoo-temp/vite.config.ts .
cp petting-zoo-temp/tsconfig.json .
cp petting-zoo-temp/tsconfig.node.json .
cp petting-zoo-temp/svelte.config.js .
cp petting-zoo-temp/index.html .
cp -r petting-zoo-temp/src/ src/
cp -r petting-zoo-temp/public/ public/
cp petting-zoo-temp/.gitignore .gitignore
rm -rf petting-zoo-temp
```

- [ ] **Step 3: Install dependencies and add Vitest**

```bash
npm install
npm install -D vitest
```

- [ ] **Step 4: Verify the scaffold works**

```bash
npm run build
```

Expected: Build succeeds, `dist/` directory created.

- [ ] **Step 5: Update .gitignore**

Ensure `.gitignore` contains at minimum:

```
node_modules/
dist/
*.local
.env
.env.*
models/
```

The `models/` entry prevents accidentally committing large GGUF files.

- [ ] **Step 6: Add test script to package.json**

Open `package.json` and add `"test": "vitest"` to the `"scripts"` section if not already present.

- [ ] **Step 7: Commit**

```bash
git add package.json package-lock.json vite.config.ts tsconfig.json tsconfig.node.json svelte.config.js index.html src/ public/ .gitignore
git commit -m "feat: scaffold fresh Svelte 5 + Vite + TypeScript project"
```

---

### Task 5: Configure Vite proxy

**Files:**
- Modify: `vite.config.ts`

- [ ] **Step 1: Update vite.config.ts with proxy configuration**

Replace the contents of `vite.config.ts` with:

```typescript
import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'

const apiTarget = process.env.VITE_API_URL ?? 'http://localhost:8080'

export default defineConfig({
  plugins: [svelte()],
  server: {
    proxy: {
      '/healthz': apiTarget,
      '/v1': apiTarget,
      '/metrics': apiTarget,
    },
  },
})
```

- [ ] **Step 2: Verify dev server starts**

```bash
npm run dev -- --host &
sleep 2
kill %1
```

Expected: Vite starts without errors. The proxy targets won't resolve yet (server not running), but the dev server itself should boot.

- [ ] **Step 3: Commit**

```bash
git add vite.config.ts
git commit -m "feat: configure Vite proxy for zoo-keeper-server API routes

Proxies /healthz, /v1/*, /metrics to VITE_API_URL (default localhost:8080)."
```

---

### Task 6: Create API client with tests (TDD)

**Files:**
- Create: `src/lib/api/types.ts`
- Create: `src/lib/api/client.ts`
- Create: `tests/lib/api/client.test.ts`

- [ ] **Step 1: Create the types file**

Create `src/lib/api/types.ts`:

```typescript
export interface HealthResponse {
  status: string
  ready: boolean
  version: string
  model_id: string
}
```

- [ ] **Step 2: Write the failing tests**

Create `tests/lib/api/client.test.ts`:

```typescript
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import type { HealthResponse } from '../../../src/lib/api/types'

// Will import once implemented
// import { fetchHealth } from '../../../src/lib/api/client'

describe('fetchHealth', () => {
  const mockHealth: HealthResponse = {
    status: 'ready',
    ready: true,
    version: '0.0.4',
    model_id: 'local-model',
  }

  beforeEach(() => {
    vi.stubGlobal('fetch', vi.fn())
  })

  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('returns parsed health data on 200', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response(JSON.stringify(mockHealth), { status: 200 })
    )

    const { fetchHealth } = await import('../../../src/lib/api/client')
    const result = await fetchHealth()

    expect(fetch).toHaveBeenCalledWith('/healthz')
    expect(result).toEqual(mockHealth)
  })

  it('throws on non-OK response', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response('Service Unavailable', { status: 503 })
    )

    const { fetchHealth } = await import('../../../src/lib/api/client')
    await expect(fetchHealth()).rejects.toThrow('Health check failed: 503')
  })

  it('throws on network error', async () => {
    vi.mocked(fetch).mockRejectedValue(new TypeError('Failed to fetch'))

    const { fetchHealth } = await import('../../../src/lib/api/client')
    await expect(fetchHealth()).rejects.toThrow('Failed to fetch')
  })
})
```

- [ ] **Step 3: Run tests to verify they fail**

```bash
npx vitest run tests/lib/api/client.test.ts
```

Expected: Tests fail because `src/lib/api/client.ts` does not exist.

- [ ] **Step 4: Implement the client**

Create `src/lib/api/client.ts`:

```typescript
import type { HealthResponse } from './types'

export async function fetchHealth(): Promise<HealthResponse> {
  const res = await fetch('/healthz')
  if (!res.ok) {
    throw new Error(`Health check failed: ${res.status}`)
  }
  return res.json()
}
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
npx vitest run tests/lib/api/client.test.ts
```

Expected: All 3 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/lib/api/types.ts src/lib/api/client.ts tests/lib/api/client.test.ts
git commit -m "feat: add typed API client for /healthz endpoint

TDD: fetchHealth() with tests for success, HTTP error, and network error."
```

---

### Task 7: Build Health.svelte component

**Files:**
- Create: `src/routes/Health.svelte`
- Modify: `src/App.svelte`
- Modify: `src/app.css`
- Delete: Default scaffold files (e.g., `src/lib/Counter.svelte`, `src/assets/`)

- [ ] **Step 1: Clean up scaffold defaults**

Remove the default Vite+Svelte scaffold content:

```bash
rm -rf src/lib/Counter.svelte src/assets/
```

- [ ] **Step 2: Create Health.svelte**

Create `src/routes/Health.svelte`:

```svelte
<script lang="ts">
  import { onMount, onDestroy } from 'svelte'
  import { fetchHealth } from '../lib/api/client'
  import type { HealthResponse } from '../lib/api/types'

  let health: HealthResponse | null = $state(null)
  let error: string | null = $state(null)
  let lastChecked: Date | null = $state(null)
  let intervalId: ReturnType<typeof setInterval> | null = null

  async function poll() {
    try {
      health = await fetchHealth()
      error = null
    } catch (e) {
      health = null
      error = e instanceof Error ? e.message : 'Unknown error'
    }
    lastChecked = new Date()
  }

  onMount(() => {
    poll()
    intervalId = setInterval(poll, 5000)
  })

  onDestroy(() => {
    if (intervalId) clearInterval(intervalId)
  })

  const statusText = $derived(
    error ? 'Unreachable' : health?.ready ? 'Ready' : 'Starting'
  )

  const statusClass = $derived(
    error ? 'red' : health?.ready ? 'green' : 'yellow'
  )
</script>

<div class="health-dashboard">
  <h1>Zoo Keeper Server</h1>

  <div class="status-row">
    <span class="dot {statusClass}"></span>
    <span class="status-text">{statusText}</span>
  </div>

  {#if health}
    <table class="details">
      <tr><td>Version</td><td>{health.version}</td></tr>
      <tr><td>Model</td><td>{health.model_id}</td></tr>
      <tr><td>Status</td><td>{health.status}</td></tr>
    </table>
  {/if}

  {#if error}
    <p class="error">{error}</p>
  {/if}

  {#if lastChecked}
    <p class="timestamp">Last checked: {lastChecked.toLocaleTimeString()}</p>
  {/if}
</div>

<style>
  .health-dashboard {
    max-width: 480px;
    margin: 2rem auto;
    font-family: system-ui, -apple-system, sans-serif;
  }

  h1 {
    font-size: 1.5rem;
    margin-bottom: 1rem;
  }

  .status-row {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    margin-bottom: 1.5rem;
  }

  .dot {
    width: 16px;
    height: 16px;
    border-radius: 50%;
    flex-shrink: 0;
  }

  .dot.green { background: #22c55e; }
  .dot.yellow { background: #eab308; }
  .dot.red { background: #ef4444; }

  .status-text {
    font-size: 1.25rem;
    font-weight: 600;
  }

  .details {
    width: 100%;
    border-collapse: collapse;
    font-family: ui-monospace, monospace;
    font-size: 0.875rem;
  }

  .details td {
    padding: 0.5rem 0.75rem;
    border-bottom: 1px solid #333;
  }

  .details td:first-child {
    color: #999;
    width: 100px;
  }

  .error {
    color: #ef4444;
    font-size: 0.875rem;
    margin-top: 1rem;
  }

  .timestamp {
    color: #666;
    font-size: 0.75rem;
    margin-top: 1rem;
  }
</style>
```

- [ ] **Step 3: Update App.svelte**

Replace `src/App.svelte` with:

```svelte
<script lang="ts">
  import Health from './routes/Health.svelte'
</script>

<main>
  <Health />
</main>
```

- [ ] **Step 4: Update app.css**

Replace `src/app.css` with:

```css
:root {
  color-scheme: dark;
}

body {
  margin: 0;
  background: #111;
  color: #e5e5e5;
}
```

- [ ] **Step 5: Update main.ts if needed**

Ensure `src/main.ts` imports `app.css` and mounts `App.svelte`:

```typescript
import './app.css'
import App from './App.svelte'
import { mount } from 'svelte'

const app = mount(App, { target: document.getElementById('app')! })

export default app
```

- [ ] **Step 6: Verify build succeeds**

```bash
npm run build
```

Expected: Build completes with no errors.

- [ ] **Step 7: Commit**

```bash
git add src/
git rm -f src/lib/Counter.svelte src/assets/ 2>/dev/null; true
git commit -m "feat: add Health dashboard component with auto-polling

Displays server status (ready/starting/unreachable) with live
polling every 5 seconds. Dark theme, minimal styling."
```

---

### Task 8: Create Docker Compose setup

**Files:**
- Create: `docker-compose.yml`
- Create: `models/.gitkeep`

- [ ] **Step 1: Create models directory**

```bash
mkdir -p models
touch models/.gitkeep
```

- [ ] **Step 2: Create docker-compose.yml**

Create `docker-compose.yml`:

```yaml
services:
  server:
    build:
      context: .
      dockerfile: Dockerfile.server
    ports:
      - "8080:8080"
    volumes:
      - ./models:/models

  frontend:
    image: node:22-alpine
    working_dir: /app
    command: sh -c "npm ci && npm run dev -- --host"
    ports:
      - "5173:5173"
    volumes:
      - .:/app
      - /app/node_modules
    environment:
      - VITE_API_URL=http://server:8080
    depends_on:
      - server

volumes:
  node_modules:
```

- [ ] **Step 3: Verify compose config parses**

```bash
docker compose config
```

Expected: Outputs the resolved YAML with no errors.

- [ ] **Step 4: Commit**

```bash
git add docker-compose.yml models/.gitkeep
git commit -m "feat: add Docker Compose for server + frontend dev

Server built from zoo-keeper-server submodule, frontend via node:22-alpine.
Mount GGUF models to ./models/."
```

---

### Task 9: Rewrite CLAUDE.md

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 1: Rewrite CLAUDE.md**

Replace the contents of `CLAUDE.md` with:

```markdown
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
```

- [ ] **Step 2: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: rewrite CLAUDE.md for frontend-only architecture"
```

---

### Task 10: Write README.md

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Rewrite README.md**

Replace the contents of `README.md` with:

```markdown
# Petting Zoo

A web frontend for [zoo-keeper-server](https://github.com/crybo-rybo/zoo-keeper-server) — local LLM inference in your browser.

## Prerequisites

- [Docker](https://docs.docker.com/get-docker/) and Docker Compose
- [Node.js 22+](https://nodejs.org/) (for local frontend development)
- A GGUF model file (place in `models/` directory)

## Quick Start

```bash
git clone --recursive https://github.com/crybo-rybo/petting-zoo.git
cd petting-zoo

# Place a GGUF model in models/
cp /path/to/your-model.gguf models/model.gguf

# Start everything
docker compose up --build
```

- Frontend: http://localhost:5173
- Server API: http://localhost:8080

## Local Frontend Development

If you prefer running the frontend natively (faster hot reload):

```bash
npm ci
npm run dev
```

This requires zoo-keeper-server running separately on `localhost:8080`.

## Testing

```bash
npx vitest run
```

## Project Structure

```
petting-zoo/
├── zoo-keeper-server/      # Git submodule — C++ inference server
├── docker-compose.yml      # Runs server + frontend together
├── Dockerfile.server       # Builds zoo-keeper-server from source
├── src/                    # Svelte frontend source
│   ├── App.svelte
│   ├── lib/api/            # Typed API client
│   └── routes/             # Page components
├── tests/                  # Vitest unit tests
└── models/                 # Mount point for GGUF model files
```
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: rewrite README for frontend-only project"
```

---

### Task 11: End-to-end verification

- [ ] **Step 1: Verify no old files remain**

```bash
# These should all return "not found"
ls apps/ cmake/ CMakeLists.txt config/app.json tests/cpp/ AGENTS.md RECOMMENDATIONS.md 2>&1
```

Expected: Every path reports "No such file or directory".

- [ ] **Step 2: Verify frontend builds**

```bash
npm run build
```

Expected: Build succeeds.

- [ ] **Step 3: Run unit tests**

```bash
npx vitest run
```

Expected: All tests pass (3 tests in `client.test.ts`).

- [ ] **Step 4: Verify Docker Compose config**

```bash
docker compose config
```

Expected: Valid YAML output, no errors.

- [ ] **Step 5: Attempt Docker Compose build (if Docker available)**

```bash
docker compose build
```

Expected: Both services build. Server build may take several minutes on first run. If no Docker is available, skip this step.

- [ ] **Step 6: Verify git status is clean**

```bash
git status
```

Expected: Working tree clean (or only untracked `node_modules/`, `dist/` which are gitignored).
