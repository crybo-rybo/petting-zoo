# Chat Interface + Sidebar Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a streaming chat interface with server-managed sessions and icon sidebar navigation to the petting-zoo frontend.

**Architecture:** Extend the existing API client with `fetchModels`, `createSession`, and `sendChatMessage` functions. Add an SSE stream parser. Build a 56px icon sidebar for view switching (Health/Chat) in `App.svelte`. Build `Chat.svelte` with full-width alternating message rows, streaming token display, and automatic session management.

**Tech Stack:** Svelte 5 (runes), Vite 6, TypeScript 5, Vitest 3

**Spec:** `docs/superpowers/specs/2026-03-31-chat-interface-design.md`

---

### File Structure

| Action | File | Responsibility |
|--------|------|---------------|
| Modify | `src/lib/api/types.ts` | Add `ModelObject`, `ModelsResponse`, `ChatMessage`, `ChatCompletionRequest`, `ChatCompletionStreamChunk`, `SessionCreateRequest`, `SessionSummary` |
| Modify | `src/lib/api/client.ts` | Add `fetchModels()`, `createSession()`, `sendChatMessage()` |
| Modify | `src/lib/api/client.test.ts` | Tests for new client functions |
| Create | `src/lib/api/sse.ts` | SSE stream parser — `parseSSEStream()` async generator |
| Create | `src/lib/api/sse.test.ts` | Tests for SSE parser |
| Create | `src/components/Sidebar.svelte` | Icon sidebar navigation component |
| Modify | `src/App.svelte` | Sidebar layout + view switching |
| Modify | `src/app.css` | Global layout styles for sidebar + main content area |
| Create | `src/routes/Chat.svelte` | Chat interface component |

---

### Task 1: Add API types

**Files:**
- Modify: `src/lib/api/types.ts`

- [ ] **Step 1: Add all new interfaces to types.ts**

Append to `src/lib/api/types.ts`:

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

- [ ] **Step 2: Verify types compile**

```bash
./scripts/typecheck
```

Expected: No errors.

- [ ] **Step 3: Commit**

```bash
git add src/lib/api/types.ts
git commit -m "feat: add TypeScript interfaces for models, chat, and sessions API"
```

---

### Task 2: Add `fetchModels` client function (TDD)

**Files:**
- Modify: `src/lib/api/client.ts`
- Modify: `src/lib/api/client.test.ts`

- [ ] **Step 1: Write failing tests for fetchModels**

Append to `src/lib/api/client.test.ts`, after the existing `fetchHealth` describe block:

```typescript
import type { ModelsResponse } from './types'

describe('fetchModels', () => {
  const mockModels: ModelsResponse = {
    object: 'list',
    data: [
      { id: 'local-model', object: 'model', owned_by: 'zoo-keeper-server' },
    ],
  }

  beforeEach(() => {
    vi.stubGlobal('fetch', vi.fn())
  })

  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('returns parsed models data on 200', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response(JSON.stringify(mockModels), { status: 200 })
    )

    const { fetchModels } = await import('./client')
    const result = await fetchModels()

    expect(fetch).toHaveBeenCalledWith('/v1/models')
    expect(result).toEqual(mockModels)
  })

  it('throws on non-OK response', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response('Not Found', { status: 404 })
    )

    const { fetchModels } = await import('./client')
    await expect(fetchModels()).rejects.toThrow('Models fetch failed: 404')
  })
})
```

Note: The existing `import type { HealthResponse }` at the top of the file needs `ModelsResponse` added to it. Update the import line to:

```typescript
import type { HealthResponse, ModelsResponse } from './types'
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
npx vitest run src/lib/api/client.test.ts
```

Expected: The `fetchModels` tests fail because the function doesn't exist.

- [ ] **Step 3: Implement fetchModels**

Add to `src/lib/api/client.ts`:

```typescript
import type { HealthResponse, ModelsResponse } from './types'

export async function fetchModels(): Promise<ModelsResponse> {
  const res = await fetch('/v1/models')
  if (!res.ok) {
    throw new Error(`Models fetch failed: ${res.status}`)
  }
  return res.json()
}
```

Update the import line at the top to include `ModelsResponse`.

- [ ] **Step 4: Run tests to verify they pass**

```bash
npx vitest run src/lib/api/client.test.ts
```

Expected: All tests pass (5 total — 3 fetchHealth + 2 fetchModels).

- [ ] **Step 5: Commit**

```bash
git add src/lib/api/client.ts src/lib/api/client.test.ts
git commit -m "feat: add fetchModels() client function with tests"
```

---

### Task 3: Add `createSession` client function (TDD)

**Files:**
- Modify: `src/lib/api/client.ts`
- Modify: `src/lib/api/client.test.ts`

- [ ] **Step 1: Write failing tests for createSession**

Append to `src/lib/api/client.test.ts`:

```typescript
import type { SessionSummary } from './types'

describe('createSession', () => {
  const mockSession: SessionSummary = {
    id: 'sess-abc123',
    object: 'session',
    model: 'local-model',
    created: 1711900000,
    last_used: 1711900000,
    expires_at: 1711900900,
  }

  beforeEach(() => {
    vi.stubGlobal('fetch', vi.fn())
  })

  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('creates a session and returns summary', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response(JSON.stringify(mockSession), { status: 201 })
    )

    const { createSession } = await import('./client')
    const result = await createSession('local-model')

    expect(fetch).toHaveBeenCalledWith('/v1/sessions', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ model: 'local-model' }),
    })
    expect(result).toEqual(mockSession)
  })

  it('throws on non-OK response', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response('Service Unavailable', { status: 503 })
    )

    const { createSession } = await import('./client')
    await expect(createSession('local-model')).rejects.toThrow(
      'Session creation failed: 503'
    )
  })
})
```

Add `SessionSummary` to the types import at the top of the test file.

- [ ] **Step 2: Run tests to verify they fail**

```bash
npx vitest run src/lib/api/client.test.ts
```

Expected: `createSession` tests fail.

- [ ] **Step 3: Implement createSession**

Add to `src/lib/api/client.ts`:

```typescript
export async function createSession(model: string): Promise<SessionSummary> {
  const res = await fetch('/v1/sessions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ model }),
  })
  if (!res.ok) {
    throw new Error(`Session creation failed: ${res.status}`)
  }
  return res.json()
}
```

Add `SessionSummary` to the import from `./types`.

- [ ] **Step 4: Run tests to verify they pass**

```bash
npx vitest run src/lib/api/client.test.ts
```

Expected: All tests pass (7 total).

- [ ] **Step 5: Commit**

```bash
git add src/lib/api/client.ts src/lib/api/client.test.ts
git commit -m "feat: add createSession() client function with tests"
```

---

### Task 4: Add `sendChatMessage` client function (TDD)

**Files:**
- Modify: `src/lib/api/client.ts`
- Modify: `src/lib/api/client.test.ts`

- [ ] **Step 1: Write failing tests for sendChatMessage**

Append to `src/lib/api/client.test.ts`:

```typescript
import type { ChatCompletionRequest } from './types'

describe('sendChatMessage', () => {
  const mockRequest: ChatCompletionRequest = {
    model: 'local-model',
    messages: [{ role: 'user', content: 'Hello' }],
    stream: true,
    session_id: 'sess-abc123',
  }

  beforeEach(() => {
    vi.stubGlobal('fetch', vi.fn())
  })

  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('sends chat request and returns raw response', async () => {
    const mockResponse = new Response('data: {}\n\n', {
      status: 200,
      headers: { 'Content-Type': 'text/event-stream' },
    })
    vi.mocked(fetch).mockResolvedValue(mockResponse)

    const { sendChatMessage } = await import('./client')
    const result = await sendChatMessage(mockRequest)

    expect(fetch).toHaveBeenCalledWith('/v1/chat/completions', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(mockRequest),
    })
    expect(result).toBe(mockResponse)
  })

  it('throws on non-OK response', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response('Service Unavailable', { status: 503 })
    )

    const { sendChatMessage } = await import('./client')
    await expect(sendChatMessage(mockRequest)).rejects.toThrow(
      'Chat request failed: 503'
    )
  })

  it('throws on 404 session not found', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response('Not Found', { status: 404 })
    )

    const { sendChatMessage } = await import('./client')
    await expect(sendChatMessage(mockRequest)).rejects.toThrow(
      'Chat request failed: 404'
    )
  })
})
```

Add `ChatCompletionRequest` to the types import.

- [ ] **Step 2: Run tests to verify they fail**

```bash
npx vitest run src/lib/api/client.test.ts
```

Expected: `sendChatMessage` tests fail.

- [ ] **Step 3: Implement sendChatMessage**

Add to `src/lib/api/client.ts`:

```typescript
export async function sendChatMessage(
  request: ChatCompletionRequest
): Promise<Response> {
  const res = await fetch('/v1/chat/completions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(request),
  })
  if (!res.ok) {
    throw new Error(`Chat request failed: ${res.status}`)
  }
  return res
}
```

Add `ChatCompletionRequest` to the import from `./types`.

- [ ] **Step 4: Run tests to verify they pass**

```bash
npx vitest run src/lib/api/client.test.ts
```

Expected: All tests pass (10 total).

- [ ] **Step 5: Commit**

```bash
git add src/lib/api/client.ts src/lib/api/client.test.ts
git commit -m "feat: add sendChatMessage() client function with tests"
```

---

### Task 5: Build SSE stream parser (TDD)

**Files:**
- Create: `src/lib/api/sse.ts`
- Create: `src/lib/api/sse.test.ts`

- [ ] **Step 1: Write failing tests for parseSSEStream**

Create `src/lib/api/sse.test.ts`:

```typescript
import { describe, it, expect } from 'vitest'
import type { ChatCompletionStreamChunk } from './types'

// Helper: create a ReadableStream from a string
function stringToStream(text: string): ReadableStream<Uint8Array> {
  const encoder = new TextEncoder()
  return new ReadableStream({
    start(controller) {
      controller.enqueue(encoder.encode(text))
      controller.close()
    },
  })
}

// Helper: create a ReadableStream that emits chunks separately
function chunksToStream(chunks: string[]): ReadableStream<Uint8Array> {
  const encoder = new TextEncoder()
  return new ReadableStream({
    start(controller) {
      for (const chunk of chunks) {
        controller.enqueue(encoder.encode(chunk))
      }
      controller.close()
    },
  })
}

describe('parseSSEStream', () => {
  const makeChunk = (
    content: string,
    finish: string | null = null,
    role?: string
  ): string => {
    const delta: Record<string, string> = {}
    if (role) delta.role = role
    if (content) delta.content = content
    const chunk: ChatCompletionStreamChunk = {
      id: 'chatcmpl-123',
      object: 'chat.completion.chunk',
      created: 1711900000,
      model: 'local-model',
      choices: [{ index: 0, delta, finish_reason: finish }],
    }
    return `data: ${JSON.stringify(chunk)}\n\n`
  }

  it('parses a single SSE chunk', async () => {
    const sseData = makeChunk('Hello', null, 'assistant') + 'data: [DONE]\n\n'
    const stream = stringToStream(sseData)

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(1)
    expect(chunks[0].choices[0].delta.content).toBe('Hello')
    expect(chunks[0].choices[0].delta.role).toBe('assistant')
  })

  it('parses multiple SSE chunks', async () => {
    const sseData =
      makeChunk('Hello', null, 'assistant') +
      makeChunk(' world') +
      makeChunk('', 'stop') +
      'data: [DONE]\n\n'
    const stream = stringToStream(sseData)

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(3)
    expect(chunks[0].choices[0].delta.content).toBe('Hello')
    expect(chunks[1].choices[0].delta.content).toBe(' world')
    expect(chunks[2].choices[0].finish_reason).toBe('stop')
  })

  it('handles chunks split across read boundaries', async () => {
    const full = makeChunk('Hi', null, 'assistant') + 'data: [DONE]\n\n'
    // Split in the middle of the JSON
    const splitPoint = Math.floor(full.length / 2)
    const stream = chunksToStream([
      full.slice(0, splitPoint),
      full.slice(splitPoint),
    ])

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(1)
    expect(chunks[0].choices[0].delta.content).toBe('Hi')
  })

  it('terminates on [DONE]', async () => {
    const sseData = makeChunk('token1') + 'data: [DONE]\n\n' + makeChunk('token2')
    const stream = stringToStream(sseData)

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(1)
    expect(chunks[0].choices[0].delta.content).toBe('token1')
  })

  it('skips empty lines', async () => {
    const sseData = '\n\n' + makeChunk('Hello', null, 'assistant') + '\n\n' + 'data: [DONE]\n\n'
    const stream = stringToStream(sseData)

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(1)
    expect(chunks[0].choices[0].delta.content).toBe('Hello')
  })
})
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
npx vitest run src/lib/api/sse.test.ts
```

Expected: Tests fail because `src/lib/api/sse.ts` doesn't exist.

- [ ] **Step 3: Implement parseSSEStream**

Create `src/lib/api/sse.ts`:

```typescript
import type { ChatCompletionStreamChunk } from './types'

export async function* parseSSEStream(
  stream: ReadableStream<Uint8Array>
): AsyncGenerator<ChatCompletionStreamChunk> {
  const reader = stream.pipeThrough(new TextDecoderStream()).getReader()
  let buffer = ''

  try {
    while (true) {
      const { done, value } = await reader.read()
      if (done) break

      buffer += value

      const parts = buffer.split('\n\n')
      // Last element may be incomplete — keep it in buffer
      buffer = parts.pop() ?? ''

      for (const part of parts) {
        const line = part.trim()
        if (!line) continue
        if (!line.startsWith('data: ')) continue

        const data = line.slice(6) // strip "data: "
        if (data === '[DONE]') return

        yield JSON.parse(data) as ChatCompletionStreamChunk
      }
    }
  } finally {
    reader.releaseLock()
  }
}
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
npx vitest run src/lib/api/sse.test.ts
```

Expected: All 5 tests pass.

- [ ] **Step 5: Run all tests**

```bash
./scripts/test
```

Expected: All tests pass (15 total).

- [ ] **Step 6: Commit**

```bash
git add src/lib/api/sse.ts src/lib/api/sse.test.ts
git commit -m "feat: add SSE stream parser for chat completion chunks

Async generator that parses text/event-stream frames, handles
partial buffering, and terminates on [DONE]."
```

---

### Task 6: Build Sidebar component

**Files:**
- Create: `src/components/Sidebar.svelte`

- [ ] **Step 1: Create the Sidebar component**

Create `src/components/Sidebar.svelte`:

```svelte
<script lang="ts">
  import { onMount, onDestroy } from 'svelte'
  import { fetchHealth } from '../lib/api/client'

  interface Props {
    activeView: 'health' | 'chat'
    onNavigate: (view: 'health' | 'chat') => void
    onHealthStatus: (ready: boolean) => void
  }

  let { activeView, onNavigate, onHealthStatus }: Props = $props()

  let healthStatus: 'green' | 'yellow' | 'red' = $state('yellow')
  let intervalId: ReturnType<typeof setInterval> | null = null

  async function pollHealth() {
    try {
      const health = await fetchHealth()
      healthStatus = health.ready ? 'green' : 'yellow'
      onHealthStatus(health.ready)
    } catch {
      healthStatus = 'red'
      onHealthStatus(false)
    }
  }

  onMount(() => {
    pollHealth()
    intervalId = setInterval(pollHealth, 5000)
  })

  onDestroy(() => {
    if (intervalId) clearInterval(intervalId)
  })
</script>

<nav class="sidebar">
  <button
    class="nav-btn"
    class:active={activeView === 'health'}
    onclick={() => onNavigate('health')}
    title="Server Health"
  >
    <span class="health-dot {healthStatus}"></span>
  </button>

  <button
    class="nav-btn"
    class:active={activeView === 'chat'}
    onclick={() => onNavigate('chat')}
    title="Chat"
  >
    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>
    </svg>
  </button>

  <button
    class="nav-btn disabled"
    title="Metrics (coming soon)"
    disabled
  >
    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M18 20V10M12 20V4M6 20v-6"/>
    </svg>
  </button>
</nav>

<style>
  .sidebar {
    width: 56px;
    background: #1a1a1a;
    border-right: 1px solid #333;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 12px 0;
    gap: 8px;
    flex-shrink: 0;
  }

  .nav-btn {
    width: 40px;
    height: 40px;
    border: none;
    border-radius: 8px;
    background: transparent;
    color: #999;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    transition: background 0.15s, color 0.15s;
  }

  .nav-btn:hover:not(.disabled) {
    background: #333;
    color: #e5e5e5;
  }

  .nav-btn.active {
    background: #333;
    color: #e5e5e5;
  }

  .nav-btn.disabled {
    color: #555;
    cursor: not-allowed;
  }

  .health-dot {
    width: 12px;
    height: 12px;
    border-radius: 50%;
  }

  .health-dot.green { background: #22c55e; }
  .health-dot.yellow { background: #eab308; }
  .health-dot.red { background: #ef4444; }
</style>
```

- [ ] **Step 2: Verify it compiles**

```bash
./scripts/typecheck
```

Expected: No errors.

- [ ] **Step 3: Commit**

```bash
git add src/components/Sidebar.svelte
git commit -m "feat: add icon sidebar navigation component

Health status dot (green/yellow/red) with 5s polling, chat icon,
and disabled metrics placeholder."
```

---

### Task 7: Wire sidebar into App.svelte

**Files:**
- Modify: `src/App.svelte`
- Modify: `src/app.css`

- [ ] **Step 1: Update app.css with layout styles**

Replace the contents of `src/app.css` with:

```css
:root {
  color-scheme: dark;
}

body {
  margin: 0;
  background: #111;
  color: #e5e5e5;
}

.app-layout {
  display: flex;
  height: 100vh;
  overflow: hidden;
}

.main-content {
  flex: 1;
  overflow-y: auto;
  min-width: 0;
}
```

- [ ] **Step 2: Update App.svelte with sidebar and view switching**

Replace the contents of `src/App.svelte` with:

```svelte
<script lang="ts">
  import Sidebar from './components/Sidebar.svelte'
  import Health from './routes/Health.svelte'

  let activeView: 'health' | 'chat' = $state('chat')
  let serverReady: boolean = $state(false)

  function handleNavigate(view: 'health' | 'chat') {
    activeView = view
  }

  function handleHealthStatus(ready: boolean) {
    serverReady = ready
  }
</script>

<div class="app-layout">
  <Sidebar
    {activeView}
    onNavigate={handleNavigate}
    onHealthStatus={handleHealthStatus}
  />
  <div class="main-content">
    {#if activeView === 'health'}
      <Health />
    {:else if activeView === 'chat'}
      <p style="padding: 2rem; color: #666;">Chat component coming soon...</p>
    {/if}
  </div>
</div>
```

Note: The Chat placeholder will be replaced in Task 9. The `serverReady` state will be passed to Chat.svelte to control input disabling.

- [ ] **Step 3: Verify build and tests pass**

```bash
npm run build && ./scripts/test
```

Expected: Build succeeds, all tests pass.

- [ ] **Step 4: Commit**

```bash
git add src/App.svelte src/app.css
git commit -m "feat: add sidebar layout with view switching

Icon sidebar on left, main content area on right. Health and Chat
views switchable via sidebar buttons. Chat placeholder for now."
```

---

### Task 8: Build Chat component

**Files:**
- Create: `src/routes/Chat.svelte`
- Modify: `src/App.svelte`

- [ ] **Step 1: Create Chat.svelte**

Create `src/routes/Chat.svelte`:

```svelte
<script lang="ts">
  import { onMount } from 'svelte'
  import { fetchModels, createSession, sendChatMessage } from '../lib/api/client'
  import { parseSSEStream } from '../lib/api/sse'
  import type { ChatMessage } from '../lib/api/types'

  interface Props {
    serverReady: boolean
  }

  let { serverReady }: Props = $props()

  // State
  let messages: Array<{ role: 'user' | 'assistant'; content: string }> = $state([])
  let inputText: string = $state('')
  let isStreaming: boolean = $state(false)
  let sessionId: string | null = $state(null)
  let modelId: string | null = $state(null)
  let error: string | null = $state(null)
  let messagesContainer: HTMLDivElement | undefined = $state(undefined)

  const inputDisabled = $derived(!serverReady || isStreaming || !modelId)
  const placeholderText = $derived(
    !serverReady
      ? 'Server unreachable'
      : !modelId
        ? 'No model loaded'
        : isStreaming
          ? 'Waiting for response...'
          : 'Type a message...'
  )

  onMount(async () => {
    try {
      const modelsResponse = await fetchModels()
      if (modelsResponse.data.length > 0) {
        modelId = modelsResponse.data[0].id
      }
    } catch {
      // Model info will show as unavailable
    }
  })

  function scrollToBottom() {
    if (messagesContainer) {
      messagesContainer.scrollTop = messagesContainer.scrollHeight
    }
  }

  async function ensureSession(): Promise<string> {
    if (sessionId) return sessionId
    if (!modelId) throw new Error('No model loaded')
    const session = await createSession(modelId)
    sessionId = session.id
    return session.id
  }

  async function handleSend() {
    const text = inputText.trim()
    if (!text || inputDisabled) return

    inputText = ''
    error = null

    // Add user message immediately
    messages.push({ role: 'user', content: text })
    // Add empty assistant message for streaming
    const assistantIndex = messages.length
    messages.push({ role: 'assistant', content: '' })

    // Scroll after adding messages
    await tick()
    scrollToBottom()

    isStreaming = true

    try {
      let currentSessionId: string
      try {
        currentSessionId = await ensureSession()
      } catch (e) {
        throw new Error(
          e instanceof Error ? e.message : 'Failed to create session'
        )
      }

      const chatMessage: ChatMessage = { role: 'user', content: text }

      let response: Response
      try {
        response = await sendChatMessage({
          model: modelId!,
          messages: [chatMessage],
          stream: true,
          session_id: currentSessionId,
        })
      } catch (e) {
        // Handle session expired — auto-recreate
        if (e instanceof Error && e.message.includes('404')) {
          sessionId = null
          const newSessionId = await ensureSession()

          // Add a notice about session expiry
          messages[assistantIndex].content =
            'Session expired — starting a new conversation.\n\n'

          response = await sendChatMessage({
            model: modelId!,
            messages: [chatMessage],
            stream: true,
            session_id: newSessionId,
          })
        } else {
          throw e
        }
      }

      if (!response.body) {
        throw new Error('No response stream')
      }

      for await (const chunk of parseSSEStream(response.body)) {
        const delta = chunk.choices[0]?.delta
        if (delta?.content) {
          messages[assistantIndex].content += delta.content
          scrollToBottom()
        }
      }
    } catch (e) {
      const errorMsg = e instanceof Error ? e.message : 'Unknown error'
      if (messages[assistantIndex].content === '') {
        messages[assistantIndex].content = `Error: ${errorMsg}`
      } else {
        messages[assistantIndex].content += `\n\nError: ${errorMsg}`
      }
      error = errorMsg
    } finally {
      isStreaming = false
      scrollToBottom()
    }
  }

  function handleKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault()
      handleSend()
    }
  }
</script>

<script module lang="ts">
  import { tick } from 'svelte'
</script>

<div class="chat">
  <div class="chat-header">
    <span class="model-label">
      {modelId ?? 'No model loaded'}
    </span>
  </div>

  <div class="chat-messages" bind:this={messagesContainer}>
    {#if messages.length === 0}
      <div class="empty-state">
        <p>Send a message to start chatting.</p>
      </div>
    {/if}

    {#each messages as msg}
      <div class="message" class:assistant={msg.role === 'assistant'}>
        <div class="message-role">
          {msg.role === 'user' ? 'You' : 'Assistant'}
        </div>
        <div class="message-content">{msg.content}</div>
      </div>
    {/each}
  </div>

  <div class="chat-input">
    <textarea
      bind:value={inputText}
      onkeydown={handleKeydown}
      placeholder={placeholderText}
      disabled={inputDisabled}
      rows="1"
    ></textarea>
    <button
      class="send-btn"
      onclick={handleSend}
      disabled={inputDisabled || !inputText.trim()}
      title="Send message"
    >
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <line x1="22" y1="2" x2="11" y2="13"/>
        <polygon points="22 2 15 22 11 13 2 9 22 2"/>
      </svg>
    </button>
  </div>
</div>

<style>
  .chat {
    display: flex;
    flex-direction: column;
    height: 100vh;
    font-family: system-ui, -apple-system, sans-serif;
  }

  .chat-header {
    padding: 12px 20px;
    border-bottom: 1px solid #333;
    display: flex;
    justify-content: flex-end;
    flex-shrink: 0;
  }

  .model-label {
    color: #666;
    font-size: 0.8rem;
    font-family: ui-monospace, monospace;
  }

  .chat-messages {
    flex: 1;
    overflow-y: auto;
    min-height: 0;
  }

  .empty-state {
    display: flex;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: #666;
  }

  .message {
    padding: 16px 20px;
    border-bottom: 1px solid #222;
  }

  .message.assistant {
    background: #1a1a1a;
  }

  .message-role {
    color: #999;
    font-size: 0.7rem;
    font-weight: 600;
    text-transform: uppercase;
    margin-bottom: 6px;
  }

  .message-content {
    color: #e5e5e5;
    line-height: 1.6;
    white-space: pre-wrap;
    word-break: break-word;
  }

  .chat-input {
    padding: 12px 16px;
    border-top: 1px solid #333;
    display: flex;
    gap: 8px;
    align-items: flex-end;
    flex-shrink: 0;
  }

  textarea {
    flex: 1;
    background: #222;
    border: 1px solid #444;
    border-radius: 8px;
    padding: 10px 12px;
    color: #e5e5e5;
    font-family: system-ui, -apple-system, sans-serif;
    font-size: 0.9rem;
    resize: none;
    line-height: 1.5;
    min-height: 40px;
    max-height: 120px;
  }

  textarea::placeholder {
    color: #666;
  }

  textarea:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  textarea:focus {
    outline: none;
    border-color: #666;
  }

  .send-btn {
    width: 40px;
    height: 40px;
    border: none;
    border-radius: 8px;
    background: #2563eb;
    color: white;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    transition: background 0.15s;
  }

  .send-btn:hover:not(:disabled) {
    background: #1d4ed8;
  }

  .send-btn:disabled {
    background: #333;
    color: #666;
    cursor: not-allowed;
  }
</style>
```

- [ ] **Step 2: Wire Chat into App.svelte**

Update `src/App.svelte` to import and render Chat:

```svelte
<script lang="ts">
  import Sidebar from './components/Sidebar.svelte'
  import Health from './routes/Health.svelte'
  import Chat from './routes/Chat.svelte'

  let activeView: 'health' | 'chat' = $state('chat')
  let serverReady: boolean = $state(false)

  function handleNavigate(view: 'health' | 'chat') {
    activeView = view
  }

  function handleHealthStatus(ready: boolean) {
    serverReady = ready
  }
</script>

<div class="app-layout">
  <Sidebar
    {activeView}
    onNavigate={handleNavigate}
    onHealthStatus={handleHealthStatus}
  />
  <div class="main-content">
    {#if activeView === 'health'}
      <Health />
    {:else if activeView === 'chat'}
      <Chat {serverReady} />
    {/if}
  </div>
</div>
```

- [ ] **Step 3: Verify build and typecheck pass**

```bash
npm run build && ./scripts/typecheck
```

Expected: No errors.

- [ ] **Step 4: Run all tests**

```bash
./scripts/test
```

Expected: All tests pass (15 total).

- [ ] **Step 5: Commit**

```bash
git add src/routes/Chat.svelte src/App.svelte
git commit -m "feat: add streaming chat interface with session management

Full-width alternating message rows, SSE token streaming,
auto-session creation, session expiry recovery, and model
display in header."
```

---

### Task 9: End-to-end verification

- [ ] **Step 1: Run full test suite**

```bash
./scripts/test
```

Expected: All 15 tests pass.

- [ ] **Step 2: Run typecheck**

```bash
./scripts/typecheck
```

Expected: No errors.

- [ ] **Step 3: Production build**

```bash
npm run build
```

Expected: Build succeeds.

- [ ] **Step 4: Verify file structure**

```bash
ls src/components/Sidebar.svelte src/routes/Chat.svelte src/lib/api/sse.ts src/lib/api/sse.test.ts
```

Expected: All files exist.

- [ ] **Step 5: Verify git status is clean**

```bash
git status
```

Expected: Working tree clean (except gitignored files).
