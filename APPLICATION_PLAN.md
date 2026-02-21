# Petting Zoo Application Plan (MVP Reset)

## Objective

Stabilize Petting Zoo as a focused MVP:

- Web UI served by backend
- Local model registration/loading
- Chat with active model through `zoo-keeper`

Everything else is explicitly deferred until this baseline is solid.

## Reset Decision

As of February 21, 2026, the project had partial implementation across multiple later-phase features (notably MCP management) while core Phase 2 architecture (session-centric + streaming runtime) remained incomplete.

To reduce scope risk and restore delivery clarity, this plan resets active scope to a minimal, production-usable local chat workflow.

## Active MVP Scope

### In

- `GET /healthz`
- `GET /api/models`
- `POST /api/models/register`
- `POST /api/models/select`
- `POST /api/chat/complete`
- `POST /api/chat/reset`
- Frontend UI for health, model management, and chat
- Correlation ID and consistent error envelope
- Basic integration + frontend unit tests

### Out (Deferred)

- Session CRUD
- WebSocket streaming chat
- Knowledge base / RAG
- Prompt profile management
- MCP connector lifecycle and tool inventory
- SQLite application-state persistence
- Packaging/installer workflow

Deferred contracts are retained in `docs/api/openapi.future.yaml` for staged reintroduction.

## Current Architecture (Post-Reset)

- Backend: Drogon C++ app with MVP routes only
- Frontend: Svelte + TypeScript single-page app
- Runtime: in-memory model registry + active `zoo::Agent`
- Static assets: Vite build served by Drogon

## MVP Exit Criteria

1. User can open app in browser from backend host.
2. User can register a valid local GGUF path.
3. User can set active model.
4. User can send prompt and receive response.
5. User can reset chat history.
6. Contract/docs/tests match behavior with no known drift.

## Delivery Plan

### Phase A: Contract and Documentation Alignment

- Publish MVP OpenAPI contract as active source: `docs/api/openapi.yaml`
- Mark websocket contract deferred: `docs/api/ws-events.md`
- Preserve previous broader contract as `docs/api/openapi.future.yaml`
- Update README and config template to MVP-only claims

### Phase B: Runtime and API Simplification

- Remove non-MVP API surface from server runtime
- Keep deterministic JSON error behavior for deferred `/api/*` routes
- Refactor server into focused modules:
  - HTTP helpers
  - runtime state (model + chat)
  - route registration in `main.cpp`

### Phase C: Frontend Simplification

- Remove non-MVP UI sections and associated logic
- Keep a single streamlined flow:
  - check health
  - register/load model
  - chat/reset

### Phase D: Verification and Hardening (MVP)

- Keep smoke test aligned to MVP routes
- Ensure frontend unit tests cover active helper logic
- Run full CTest flow successfully

## Reintroduction Backlog (After MVP Stability)

Reintroduce features one vertical slice at a time in this order:

1. Session model + synchronous session chat
2. WebSocket stream transport for active request
3. SQLite state persistence for models/sessions
4. Prompt profile management
5. KB ingestion + citation payloads
6. MCP connector lifecycle integrated with chat runtime

Each slice must satisfy:

- Contract updated first
- Backend + frontend behavior implemented
- Integration tests and docs updated

## Guardrails

- Keep `zoo-keeper` as source of truth for core inference/runtime logic.
- Do not add UI-to-runtime shortcuts that bypass backend API contracts.
- Avoid broad scaffolding; ship one complete vertical slice at a time.
