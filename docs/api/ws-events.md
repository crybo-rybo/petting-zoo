# WebSocket Event Contract

This document defines the streaming contract for `WS /api/chat/{sessionId}/stream`.

## Connection
- Endpoint: `/api/chat/{sessionId}/stream`
- Optional query params:
  - `request_id`: subscribe to one in-flight request only
  - `from_seq`: request replay/resume starting from an event sequence number
- Correlation:
  - Client may send `X-Correlation-Id` during upgrade.
  - Server includes `correlation_id` on every event.

## Envelope
All events MUST use this envelope:

```json
{
  "type": "token.delta",
  "seq": 42,
  "timestamp": "2026-02-17T15:10:34Z",
  "session_id": "sess_01J...",
  "request_id": "req_01J...",
  "correlation_id": "cor_01J...",
  "payload": {}
}
```

Fields:
- `type` (string): event name
- `seq` (integer): monotonically increasing sequence within `request_id`
- `timestamp` (RFC3339 UTC string)
- `session_id` (string)
- `request_id` (string)
- `correlation_id` (string)
- `payload` (object): type-specific body

## Event Types

### `stream.started`
Sent once per request when stream subscription is active.

Payload:
```json
{
  "model_id": "llama3-8b-instruct-q4",
  "started_at": "2026-02-17T15:10:34Z"
}
```

### `token.delta`
Token/text fragment from the assistant.

Payload:
```json
{
  "delta": "Hello",
  "index": 0
}
```

### `tool.call`
Tool invocation produced by the runtime.

Payload:
```json
{
  "tool_call_id": "call_123",
  "tool_name": "search_docs",
  "arguments": {"query": "phase zero"}
}
```

### `tool.result`
Result from tool execution.

Payload:
```json
{
  "tool_call_id": "call_123",
  "ok": true,
  "content": "Found 2 documents"
}
```

### `retrieval.citations`
RAG provenance used for generation.

Payload:
```json
{
  "chunks": [
    {
      "id": "doc-a:3",
      "source": "doc-a",
      "score": 0.87,
      "preview": "..."
    }
  ]
}
```

### `response.completed`
Terminal success event.

Payload:
```json
{
  "text": "Final response text",
  "usage": {
    "prompt_tokens": 321,
    "completion_tokens": 104,
    "total_tokens": 425
  },
  "metrics": {
    "latency_ms": 1143,
    "time_to_first_token_ms": 220,
    "tokens_per_second": 28.4
  }
}
```

### `response.error`
Terminal failure event.

Payload:
```json
{
  "code": "APP-UPSTREAM-001",
  "message": "Model generation failed",
  "retryable": true,
  "details": {
    "zoo_error_code": 203
  }
}
```

### `stream.keepalive`
Optional heartbeat event for long idle periods.

Payload:
```json
{
  "interval_ms": 15000
}
```

## Ordering and Lifecycle Rules
- Event order for successful requests:
  1. `stream.started`
  2. Zero or more: `retrieval.citations`, `tool.call`, `tool.result`, `token.delta`, `stream.keepalive`
  3. `response.completed`
- Event order for failed requests:
  1. `stream.started` (if stream established)
  2. Zero or more in-flight events
  3. `response.error`
- `response.completed` and `response.error` are mutually exclusive terminal events.
- `seq` must be contiguous for all events sent by the server.

## Backpressure and Limits
- If client falls behind, server MAY close with code `4008` (`slow_consumer`).
- Server MAY truncate oversized payload fields and indicate truncation in payload metadata.

## Suggested Close Codes
- `1000`: normal completion
- `1001`: server shutdown
- `1008`: policy violation / invalid session
- `1011`: internal server error
- `4001`: unauthorized
- `4004`: session not found
- `4008`: slow consumer

## Compatibility
- Additive event fields are backward-compatible.
- New event types must be ignored by clients that do not recognize them.
- Breaking changes require minor API version bump and release notes.
