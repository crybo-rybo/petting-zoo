# Chat Interface + Sidebar Navigation — Design Spec

## Context

Petting Zoo is a Svelte 5 frontend for zoo-keeper-server. The frontend reboot is complete with a health dashboard. The next feature is a chat interface — the core use case of the app. This also introduces sidebar navigation since the app now has multiple views.

The server currently loads a single model at startup (no dynamic model switching — tracked in zoo-keeper-server#16). The chat interface works with whatever model is loaded.

## Decisions

- **Icon sidebar** (56px) for navigation — minimal footprint, extensible
- **Server-managed sessions** — session auto-created on first message, server handles history
- **Full-width alternating rows** for messages — ChatGPT/Claude style, better for long responses
- **SSE streaming** — tokens render in real-time as they arrive
- **No markdown rendering** in this milestone — plain text only

## Project Structure (new/modified files)

```
src/
  App.svelte              — Modified: sidebar layout + view switching
  app.css                 — Modified: sidebar + chat styles
  components/
    Sidebar.svelte        — New: icon sidebar navigation
  lib/api/
    types.ts              — Modified: add chat, session, model types
    client.ts             — Modified: add fetchModels, createSession, sendChatMessage
    client.test.ts        — Modified: tests for new client functions
    sse.ts                — New: SSE stream parser
    sse.test.ts           — New: SSE parser tests
  routes/
    Health.svelte         — Unchanged
    Chat.svelte           — New: chat interface component
```

## Navigation: Icon Sidebar

A 56px-wide vertical sidebar on the left edge with icon buttons:

- **Health** — colored status dot (green/yellow/red) that doubles as nav + live indicator. Polls `/healthz` every 5s regardless of active view.
- **Chat** — chat icon, default active view
- **Metrics** — placeholder for future (disabled/greyed, not functional)

Active view is tracked by a `$state` variable in `App.svelte` (`'health' | 'chat'`). Views rendered with `{#if}/{:else if}` blocks — no router library.

The sidebar background is `#1a1a1a` with a `1px solid #333` right border, matching the dark theme.

## Chat Component (`Chat.svelte`)

### Layout

Three vertical sections within the main content area:

1. **Header bar** — shows current model ID (from `GET /v1/models`), right-aligned. Minimal height.
2. **Message area** — scrollable, takes remaining vertical space. Full-width alternating rows.
3. **Input area** — fixed at bottom. Text area with send button.

### Message Display

Full-width rows, alternating style:

- **User messages**: role label "You" (uppercase, small, `#999`), message text below. Background `#111` (same as page).
- **Assistant messages**: role label "Assistant", message text below. Background `#1a1a1a` (subtle differentiation). Separated by `1px solid #222` borders.
- Auto-scroll to bottom as new tokens stream in during generation.
- Plain text only — no markdown rendering in this milestone.

### Input Area

- Multi-line `<textarea>` with placeholder "Type a message..."
- **Enter** sends the message, **Shift+Enter** inserts a newline
- Send button to the right of the textarea (arrow icon or "Send" text)
- Input is disabled while a response is streaming
- Input is disabled when the server is unreachable (health check failing)

### Session Lifecycle

1. On first message send, call `createSession(modelId)` to get a `session_id`
2. Store `session_id` in component state
3. Each subsequent message sends `{ model, messages: [{ role: "user", content }], stream: true, session_id }`
4. If server returns `404 session_not_found`, auto-create a new session and retry. Display a notice that the previous conversation history was lost.
5. Session is per-component-lifetime — navigating away from Chat and back starts fresh

### Streaming Flow

1. User submits message → add user message row to display immediately
2. Add empty assistant message row (will fill as tokens arrive)
3. Call `sendChatMessage()` which returns a `ReadableStream`
4. Parse SSE frames using the `sse.ts` parser
5. For each `ChatCompletionStreamChunk`: append `delta.content` to the assistant message
6. On `[DONE]`: mark generation complete, re-enable input
7. On error mid-stream: show error text in the assistant message row

## API Layer

### New Types (`types.ts`)

```typescript
// Models
export interface ModelObject {
  id: string
  object: string
  owned_by: string
}

export interface ModelsResponse {
  object: string
  data: ModelObject[]
}

// Messages
export interface ChatMessage {
  role: 'system' | 'user' | 'assistant'
  content: string
}

// Chat completion request
export interface ChatCompletionRequest {
  model: string
  messages: ChatMessage[]
  stream?: boolean
  session_id?: string
}

// Streaming chunk
export interface ChatCompletionStreamChunk {
  id: string
  object: string
  created: number
  model: string
  choices: Array<{
    index: number
    delta: {
      role?: string
      content?: string
    }
    finish_reason: string | null
  }>
}

// Sessions
export interface SessionCreateRequest {
  model: string
}

export interface SessionSummary {
  id: string
  object: string
  model: string
  created: number
  last_used: number
  expires_at: number
}
```

Note: `ChatCompletionRequest` omits optional sampling params (temperature, top_p, etc.) for this milestone. Server defaults are used.

### New Client Functions (`client.ts`)

```typescript
export async function fetchModels(): Promise<ModelsResponse>
// GET /v1/models — same pattern as fetchHealth()

export async function createSession(model: string): Promise<SessionSummary>
// POST /v1/sessions with { model }
// Throws on non-2xx

export function sendChatMessage(request: ChatCompletionRequest): Promise<Response>
// POST /v1/chat/completions with stream: true
// Returns the raw Response so the caller can read the SSE stream
// Throws on non-2xx status (before streaming begins)
```

`sendChatMessage` returns the raw `Response` (not parsed JSON) because the caller needs access to `response.body` (a `ReadableStream`) for SSE parsing. Error checking happens on the status code before reading the stream.

### SSE Parser (`sse.ts`)

```typescript
export async function* parseSSEStream(
  stream: ReadableStream<Uint8Array>
): AsyncGenerator<ChatCompletionStreamChunk>
```

- Reads the stream with a `TextDecoderStream`
- Splits on `\n\n` to get SSE frames
- Strips `data: ` prefix from each frame
- Yields parsed `ChatCompletionStreamChunk` objects
- Terminates on `data: [DONE]`
- Handles partial chunks (buffering across read boundaries)

## Error Handling

| Scenario | Behavior |
|----------|----------|
| Server unreachable (health poll fails) | Sidebar health dot turns red. Chat input disabled with "Server unreachable" placeholder. |
| Session expired (`404 session_not_found`) | Auto-create new session, retry the message. Show notice: "Session expired — starting a new conversation." |
| Session busy (`409`) | Should not happen (input disabled during streaming). If it does, show error in chat. |
| Queue full (`503`) | Show "Server busy, try again" as an error message in the chat area. Re-enable input. |
| Stream error (network drop mid-stream) | Show error text in the assistant message row. Re-enable input. |
| Model not loaded (`GET /v1/models` returns empty `data`) | Show "No model loaded" in header. Disable input. |

## Testing

### Unit Tests

- `client.test.ts`: Tests for `fetchModels()`, `createSession()`, `sendChatMessage()` — success and error cases with mocked fetch
- `sse.test.ts`: Tests for `parseSSEStream()` — single chunk, multiple chunks, partial buffering, `[DONE]` termination, malformed data

### Manual Integration Test

1. `docker compose up --build`
2. Open `http://localhost:5173`
3. Verify sidebar appears with health dot (green) and chat icon
4. Click chat → see empty chat with model name in header
5. Type a message, press Enter → see user message appear immediately
6. See assistant response stream in token-by-token
7. Send follow-up message → verify conversation continues (session history works)
8. Stop server container → health dot turns red, input disabled
9. Restart server → health dot turns green, input re-enabled (new session needed)

## What This Does NOT Include

- Markdown or code block rendering — plain text only
- Message editing, regeneration, or deletion
- localStorage persistence of messages
- "New chat" button — session is per-component-lifetime
- Typing/loading indicator animation
- Sampling parameter controls (temperature, etc.)
- System prompt customization
