# Petting Zoo

Petting Zoo is an application layer for the [`zoo-keeper`](https://github.com/crybo-rybo/zoo-keeper.git) C++ library. It provides a local app experience with:

- A Drogon-based C++ API server
- A Svelte + TypeScript web UI
- Streaming chat over WebSocket
- Session, model, prompt, memory, and knowledge-base management
- MCP connector support for external tools/resources

## Status
Planning and scaffolding stage. See `APPLICATION_PLAN.md` for phased implementation details.

## Goals
- Keep core inference logic in `zoo-keeper`
- Provide a clean API boundary between UI and runtime
- Ship incrementally with testable milestones
- Support local-first workflows and robust observability

## Planned Stack
- Backend: C++ + Drogon
- Frontend: Svelte + TypeScript (Vite)
- Persistence: SQLite + `zoo-keeper` context DB
- Protocols: REST + WebSocket + MCP (JSON-RPC 2.0)

## Planned Repository Structure
- `apps/server/` API host
- `apps/web/` UI app
- `include/zoo_app/` app interfaces
- `src/zoo_app/` app implementations
- `config/` runtime config
- `docs/api/` API and stream contracts

## Primary Milestones
1. Contracts and skeleton
2. Core model/session/chat runtime
3. Knowledge + memory workflows
4. Prompt management
5. MCP connectors
6. Hardening and packaging

## Development Principles
- API contract-first development (OpenAPI + stream schema)
- Vertical slices over broad incomplete scaffolds
- Strict definition of done: tests, telemetry, docs, and UX parity

## Related Project
- `zoo-keeper`: https://github.com/crybo-rybo/zoo-keeper.git
