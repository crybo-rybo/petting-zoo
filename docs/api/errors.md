# Error Model and Taxonomy

The application uses a consistent JSON error envelope for REST and stream failures.

## Envelope

```json
{
  "error": {
    "code": "APP-VAL-001",
    "category": "validation",
    "message": "Invalid request payload",
    "retryable": false,
    "correlation_id": "cor_01J...",
    "details": {
      "field": "message"
    }
  }
}
```

## Categories
- `validation`: malformed or semantically invalid client input
- `auth`: authentication/authorization failures
- `not_found`: missing resource
- `conflict`: state conflict or idempotency collision
- `rate_limit`: quotas or throttling
- `timeout`: operation deadline exceeded
- `upstream`: model/backend/MCP dependency failure
- `internal`: unexpected server faults

## HTTP Mapping
- 400: validation
- 401/403: auth
- 404: not_found
- 409: conflict
- 413: payload too large
- 429: rate_limit
- 500: internal
- 502/503: upstream
- 504: timeout

## Standard App Error Codes
- `APP-VAL-001`: invalid request body
- `APP-VAL-002`: invalid query/path parameter
- `APP-SES-404`: session not found
- `APP-MOD-404`: model not found
- `APP-MOD-409`: model switching conflict
- `APP-CHAT-409`: session already has in-flight request
- `APP-KB-413`: uploaded file too large
- `APP-KB-404`: knowledge document not found
- `APP-MCP-404`: connector not found
- `APP-MCP-409`: duplicate connector name
- `APP-UPSTREAM-001`: model inference failure
- `APP-UPSTREAM-002`: MCP connector invocation failure
- `APP-TIMEOUT-001`: generation timeout
- `APP-INT-001`: unknown internal error

## Mapping from `zoo-keeper` ErrorCode
When surfacing `zoo::ErrorCode`, preserve the numeric code in `details.zoo_error_code`.

| zoo::ErrorCode | App code | Category | Retryable |
| --- | --- | --- | --- |
| `InvalidConfig (100)` | `APP-VAL-001` | validation | false |
| `InvalidModelPath (101)` | `APP-MOD-404` | not_found | false |
| `InvalidContextSize (102)` | `APP-VAL-001` | validation | false |
| `InvalidTemplate (103)` | `APP-VAL-001` | validation | false |
| `BackendInitFailed (200)` | `APP-UPSTREAM-001` | upstream | true |
| `ModelLoadFailed (201)` | `APP-UPSTREAM-001` | upstream | true |
| `ContextCreationFailed (202)` | `APP-UPSTREAM-001` | upstream | true |
| `InferenceFailed (203)` | `APP-UPSTREAM-001` | upstream | true |
| `TokenizationFailed (204)` | `APP-UPSTREAM-001` | upstream | true |
| `ContextWindowExceeded (300)` | `APP-VAL-001` | validation | false |
| `InvalidMessageSequence (301)` | `APP-VAL-001` | validation | false |
| `TemplateRenderFailed (302)` | `APP-INT-001` | internal | false |
| `HistoryCorrupted (303)` | `APP-INT-001` | internal | false |
| `AgentNotRunning (400)` | `APP-UPSTREAM-001` | upstream | true |
| `RequestCancelled (401)` | `APP-CHAT-409` | conflict | true |
| `RequestTimeout (402)` | `APP-TIMEOUT-001` | timeout | true |
| `QueueFull (403)` | `APP-CHAT-409` | conflict | true |
| `ToolNotFound (500)` | `APP-UPSTREAM-002` | upstream | false |
| `ToolExecutionFailed (501)` | `APP-UPSTREAM-002` | upstream | true |
| `InvalidToolSignature (502)` | `APP-UPSTREAM-002` | upstream | false |
| `ToolRetriesExhausted (503)` | `APP-UPSTREAM-002` | upstream | true |
| `ToolLoopLimitReached (504)` | `APP-UPSTREAM-002` | upstream | false |
| `Unknown (999)` | `APP-INT-001` | internal | false |

## Correlation ID Policy
- Accept client-supplied `X-Correlation-Id` when present.
- Generate one when absent.
- Return it in response header and error payload.
- Include it in every structured log and websocket event.
