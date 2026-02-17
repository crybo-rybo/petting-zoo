# AGENTS.md

## Purpose
Guidance for contributors and coding agents working in this repository.

## Project Context
- This repo hosts the Petting Zoo application built on top of `zoo-keeper`.
- `zoo-keeper` remains the source of truth for core inference/runtime logic.
- Application responsibilities here include API hosting, UI, app state, orchestration, and integrations.

## Source of Truth
- Implementation roadmap: `APPLICATION_PLAN.md`
- High-level project overview: `README.md`

## Working Rules
- Follow contract-first development:
  - Define/modify API contracts before implementation.
  - Keep REST and WebSocket schemas synchronized with behavior.
- Build incrementally in vertical slices.
- Maintain clean separation:
  - Frontend communicates only with backend APIs.
  - App layer should not duplicate core `zoo-keeper` logic.
- Keep protocol/config versions explicit and configurable (especially MCP).

## Quality Bar
- Every feature change should include:
  - Tests (unit + integration minimum)
  - Logging/telemetry hooks
  - Documentation updates when behavior changes
- Prefer deterministic behavior over implicit side effects.
- Add safeguards for process lifecycle and timeout handling.

## Expected Layout (Target)
- `apps/server/`
- `apps/web/`
- `include/zoo_app/`
- `src/zoo_app/`
- `config/`
- `docs/api/`
- `tests/`

## Pull Request Expectations
- Keep changes scoped to one milestone or vertical slice when possible.
- Include clear verification steps in PR description.
- Highlight API contract changes explicitly.

## Non-Goals
- Re-implementing model/runtime internals already provided by `zoo-keeper`.
- Introducing UI-to-runtime shortcuts that bypass backend contracts.
