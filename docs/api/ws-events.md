# WebSocket Event Contract (Deferred)

WebSocket streaming is deferred in the MVP reset.

- Current MVP chat contract is synchronous: `POST /api/chat/complete`
- Current MVP history reset contract: `POST /api/chat/reset`
- Deferred streaming and session-based API contract is preserved in:
  - `docs/api/openapi.future.yaml`

When session-based streaming is reintroduced, this document should be reinstated with the active event schema and sequencing rules.
