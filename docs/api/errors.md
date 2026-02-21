# Error Model and Taxonomy (MVP)

The MVP reset uses a consistent JSON error envelope for REST failures.

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
- `not_found`: missing resource
- `conflict`: state conflict
- `upstream`: zoo-keeper/model backend failure
- `internal`: unexpected server faults

## HTTP Mapping (MVP)

- `400`: validation
- `404`: not_found
- `409`: conflict
- `500`: internal
- `502`: upstream

## Standard App Error Codes (MVP)

- `APP-VAL-001`: invalid request body
- `APP-MOD-404`: model not found
- `APP-STATE-409`: no active model loaded / invalid runtime state
- `APP-UPSTREAM-001`: model inference or backend failure
- `APP-ASSET-404`: static asset not found
- `APP-INT-001`: unknown internal error
- `APP-NOT-IMPL-001`: endpoint outside MVP reset scope

## Correlation ID Policy

- Accept client-supplied `X-Correlation-Id` when present.
- Generate one when absent.
- Return it in response header and error payload.
