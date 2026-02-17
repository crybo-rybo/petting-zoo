# Petting Zoo Application Implementation Plan

## Objective
Build a local-first application around the `zoo-keeper` C++ library with an API server, browser UI, and operational tooling that can be shipped incrementally.

## Scope and Guardrails
- Keep inference and orchestration logic in `zoo-keeper` (no logic fork in app layer).
- Build a clean app boundary: web app talks only to backend API.
- Deliver usable value at the end of every phase.
- Keep protocol versions configurable (especially MCP).
- Prefer small vertical slices over broad unfinished scaffolding.

## Target Architecture
- Backend: Drogon (C++)
- Frontend: Svelte + TypeScript + Vite
- Transport:
  - REST for stateful CRUD and control operations
  - WebSocket for token/event streaming
- Persistence:
  - SQLite for application state
  - Existing context DB from `zoo-keeper` for long-context memory

## Proposed Repository Layout
- `apps/server/` Drogon host app + API controllers
- `apps/web/` Svelte frontend
- `include/zoo_app/` app-layer interfaces/contracts
- `src/zoo_app/` app-layer implementations
- `config/` runtime config and connector templates
- `docs/api/` OpenAPI, websocket event schema, examples
- `tests/` integration and end-to-end tests

## Delivery Strategy
Implement in seven phases with explicit entry/exit criteria.

### Phase 0: Contracts and Foundations (Week 0-1)
Purpose: Prevent frontend/backend drift and de-risk integration early.

Work items:
- Define OpenAPI spec for all REST endpoints.
- Define websocket event schema for chat stream.
- Define canonical error envelope and error code taxonomy.
- Define config model (`app.yaml` + env overrides).
- Define correlation ID and request tracing policy.

Deliverables:
- `docs/api/openapi.yaml`
- `docs/api/ws-events.md`
- `docs/api/errors.md`
- `config/app.example.yaml`

Exit criteria:
- API lint passes.
- Example requests/responses exist for every endpoint.
- Backend and frontend teams can begin in parallel.

### Phase 1: App Skeleton (Week 1)
Purpose: Create runnable end-to-end shell.

Work items:
- Add `apps/server` Drogon executable.
- Add `apps/web` Svelte app with Vite build.
- Add CMake integration to copy/serve web static assets from backend.
- Add health endpoint and simple home page.
- Add dev scripts for local startup.

Deliverables:
- `zoo_app_server` executable serving static frontend assets.
- `GET /healthz` and frontend shell route.

Exit criteria:
- `cmake --build` produces server + web assets.
- Opening app in browser shows UI from backend host.

### Phase 2: Core Runtime (Week 1-2)
Purpose: First usable chat app.

Work items:
- Implement `ModelManager` with model discovery + active model selection.
- Implement `ChatSessionManager` with session CRUD.
- Add `POST /api/chat/:sessionId/send`.
- Add `WS /api/chat/:sessionId/stream` token streaming.
- Integrate `zoo-keeper` `Agent` and context DB APIs.
- Frontend: model picker, session list, basic chat panel, streaming output.

Deliverables:
- API endpoints: models/sessions/chat working.
- Functional chat UI with stream rendering.

Exit criteria:
- User can select a model, create session, send prompt, receive streamed response.
- Integration tests cover session lifecycle and streaming handshake.

### Phase 3: Knowledge + Memory (Week 2)
Purpose: Add RAG and persistent user memory workflows.

Work items:
- Implement `KnowledgeBaseService` document ingest pipeline:
  - upload, parse, chunk, embed/index
- Add `GET/DELETE` document management endpoints.
- Implement `MemoryService` for ad-hoc memory entries.
- Add retrieval provenance payload in chat responses/events.
- Frontend: KB upload, document list, citation/provenance display.

Deliverables:
- API endpoints: `/api/kb/upload`, `/api/kb/docs`, delete doc.
- Indexed retrieval integrated into response generation.

Exit criteria:
- Uploaded docs influence answers.
- UI can show at least source doc + chunk reference metadata.

### Phase 4: Prompt Management (Week 2-3)
Purpose: Make prompt behavior controllable and transparent.

Work items:
- Implement `PromptProfileService` for presets and overrides.
- Add prompt CRUD/assignment endpoints.
- Enforce per-session system prompt resolution order.
- Frontend: prompt editor and preset selector.

Deliverables:
- `GET/PUT /api/prompts/:sessionId` plus preset APIs.
- Persisted prompt config in SQLite.

Exit criteria:
- Prompt changes affect subsequent generations deterministically.
- Regression tests verify prompt precedence rules.

### Phase 5: MCP Connectors (Week 3)
Purpose: Enable external tool/resource integration.

Work items:
- Implement `McpHostService` with protocol version config (default `2025-06-18`).
- Implement connector registry in SQLite.
- Ship `stdio` connector lifecycle manager first.
- Add capability negotiation and health checks.
- Map MCP tools/resources into runtime tool-calling flow.
- Frontend: connector management screen.

Deliverables:
- `/api/mcp/connectors` CRUD and health state.
- Working stdio connector invocation path.

Exit criteria:
- At least one MCP server can be registered and used in a chat run.
- Failure states are visible via API and UI.

### Phase 6: Hardening and Ops (Week 4)
Purpose: Production-grade local app behavior.

Work items:
- Structured logging with correlation/request/session IDs.
- Timeouts, retries, and resource limits for connector/process isolation.
- Graceful shutdown and recovery paths.
- App state export/import.
- Security baseline: input validation, path/file safeguards, secret handling.

Deliverables:
- Operational runbook and troubleshooting docs.
- Configurable limits and fault-handling policies.

Exit criteria:
- Soak test passes for long sessions and repeated connector restarts.
- No unhandled crashes in defined failure scenarios.

### Phase 7: Packaging and Release (Week 4+)
Purpose: Make installation and adoption straightforward.

Work items:
- Build release artifacts per target platform.
- Add one-command local startup script.
- Finalize README, quickstart, and architecture docs.
- Add sample configs and demo workflow.

Deliverables:
- Tagged release candidate.
- Installer/startup workflow validated on clean environment.

Exit criteria:
- New user can install and run first chat flow with docs only.

## Incremental Backlog by Vertical Slice
1. Health + static frontend served from Drogon.
2. Model list/select + simple UI card.
3. Session create/list/delete + session sidebar.
4. Chat send + websocket stream render.
5. KB upload + retrieval citations in response.
6. Prompt preset edit/apply per session.
7. MCP stdio connector add/test/use.
8. Logs, limits, export/import, packaging.

## Testing Strategy
- Unit tests:
  - Service-level logic for managers/services.
  - Error mapping and validation.
- Integration tests:
  - API + SQLite interactions.
  - Chat stream contract tests (event sequence/order).
  - MCP connector lifecycle tests.
- End-to-end tests:
  - Browser-driven chat flow.
  - KB upload and retrieval visibility.
- Non-functional:
  - Latency/throughput baseline for streaming.
  - Soak tests for long-running sessions.

## Definition of Done (Per Feature)
- API contract documented and implemented.
- Error handling + telemetry present.
- Tests added (unit + integration minimum).
- Frontend flow implemented where applicable.
- Docs updated (README or feature docs).

## Major Risks and Mitigations
- Risk: Contract drift between frontend/backend.
  - Mitigation: OpenAPI/ws schema as source of truth, CI validation.
- Risk: Streaming complexity and race conditions.
  - Mitigation: Explicit stream state machine and contract tests.
- Risk: MCP process instability.
  - Mitigation: Supervisor with timeout, restart policy, health checks.
- Risk: Retrieval quality variance.
  - Mitigation: Chunking/embedding benchmarks and observable provenance.

## Immediate Next Actions
1. Create `docs/api/openapi.yaml` and websocket event schema.
2. Scaffold `apps/server` and `apps/web` with build integration.
3. Implement Phase 2 vertical slice (model/session/chat) behind stable API contracts.
