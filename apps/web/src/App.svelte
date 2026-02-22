<script lang="ts">
  import { metricsSummary, usageSummary } from './lib/chat_format';

  // State
  let modelPath = '';
  let activeModelId: string | null = null;
  let busy = false;
  let apiError = '';

  // Chat State
  type ChatMessage = { role: 'user' | 'assistant'; content: string };
  let chatHistory: ChatMessage[] = [];
  let chatInput = '';
  let chatError = '';
  let chatUsage = '';
  let chatMetrics = '';
  let chatStreaming = false;

  async function requestJson<T>(input: RequestInfo, init?: RequestInit): Promise<T> {
    const res = await fetch(input, init);
    if (!res.ok) {
      let message = `status ${res.status}`;
      try {
        const body = await res.json();
        if (body?.error?.message) {
          message = body.error.message;
        }
      } catch {
        // Fallback to status text
      }
      throw new Error(message);
    }
    return (await res.json()) as T;
  }

  async function initialize() {
    apiError = '';
    busy = true;
    try {
      const body = await requestJson<{ active_model_id: string | null }>('/api/models');
      activeModelId = body.active_model_id ?? null;
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function loadAndSelectModel() {
    if (!modelPath.trim()) return;
    apiError = '';
    chatError = '';
    busy = true;
    try {
      const regResp = await requestJson<{ model: { id: string } }>('/api/models/register', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path: modelPath.trim(), display_name: '' })
      });
      
      const selResp = await requestJson<{ active_model: { id: string } }>('/api/models/select', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ model_id: regResp.model.id })
      });
      activeModelId = selResp.active_model.id;
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function unloadModel() {
    apiError = '';
    chatError = '';
    busy = true;
    try {
      await requestJson<{ status: string }>('/api/models/unload', { method: 'POST' });
      activeModelId = null;
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function sendChat() {
    if (!chatInput.trim() || !activeModelId) return;

    chatHistory = [...chatHistory, { role: 'user', content: chatInput.trim() }];
    chatInput = '';
    chatError = '';
    chatUsage = '';
    chatMetrics = '';
    busy = true;
    chatStreaming = true;

    chatHistory = [...chatHistory, { role: 'assistant', content: '' }];
    
    try {
      const latestMessage = chatHistory[chatHistory.length - 2].content;

      const resp = await fetch('/api/chat/stream', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message: latestMessage })
      });

      if (!resp.ok) {
        let message = `status ${resp.status}`;
        try {
          const body = await resp.json();
          if (body?.error?.message) message = body.error.message;
        } catch { /* ignore */ }
        throw new Error(message);
      }
      if (!resp.body) throw new Error('No response body');

      const reader = resp.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const parts = buffer.split('\n\n');
        buffer = parts.pop() ?? '';

        for (const part of parts) {
          if (!part.startsWith('data: ')) continue;
          const event = JSON.parse(part.slice(6));

          if (event.type === 'token' && event.content !== undefined) {
             chatHistory[chatHistory.length - 1].content += event.content;
             chatHistory = [...chatHistory]; // trigger reactivity
          } else if (event.type === 'done') {
            if (event.usage) chatUsage = usageSummary(event.usage);
            if (event.metrics) chatMetrics = metricsSummary(event.metrics);
          } else if (event.type === 'error') {
            chatError = event.message ?? event.code ?? 'error';
          }
        }
      }
    } catch (e) {
      chatError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
      chatStreaming = false;
    }
  }

  async function resetChat() {
    apiError = '';
    chatError = '';
    busy = true;
    try {
      await requestJson('/api/chat/reset', { method: 'POST' });
      chatHistory = [];
      chatUsage = '';
      chatMetrics = '';
    } catch (e) {
      chatError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function wipeMemory() {
    if (!confirm("Are you sure you want to permanently clear the model's long-term memory?")) return;
    apiError = '';
    chatError = '';
    busy = true;
    try {
      await requestJson('/api/chat/clear_memory', { method: 'POST' });
      chatHistory = [];
      chatUsage = '';
      chatMetrics = '';
    } catch (e) {
      chatError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  initialize();
</script>

<main class="shell">
  <section class="hero">
    <h1>Petting Zoo</h1>
  </section>

  <!-- Model loading / unloading -->
  <section class="card model-controls">
    <div class="model-input-row">
      <input 
        bind:value={modelPath} 
        placeholder="/absolute/path/to/model.gguf" 
        disabled={busy || activeModelId !== null} 
      />
      {#if !activeModelId}
        <button on:click={loadAndSelectModel} disabled={busy || !modelPath.trim()}>Load</button>
      {:else}
        <button class="danger" on:click={unloadModel} disabled={busy}>Unload</button>
      {/if}
    </div>
    {#if activeModelId}
      <div class="active-status-row">
        <p class="status-text">Active model: <span class="mono">{activeModelId}</span></p>
        <div class="badge-memory" title="Long-term context database is active">🧠 Memory Active</div>
      </div>
    {/if}
  </section>

  <section class="card chat-container">
    <div class="chat-history">
      {#if chatHistory.length === 0}
        <p class="empty-state">No messages yet. Send a message to start.</p>
      {:else}
        {#each chatHistory as msg, i}
          <div class="message {msg.role}">
            <strong>{msg.role === 'user' ? 'You' : 'Model'}</strong>
            <pre>{msg.content}{#if chatStreaming && i === chatHistory.length - 1}<span class="cursor">▌</span>{/if}</pre>
          </div>
        {/each}
      {/if}
    </div>
    
    {#if chatError}
      <p class="error">Chat error: {chatError}</p>
    {/if}
    
    <div class="chat-input-row">
      <textarea 
        bind:value={chatInput} 
        rows="2" 
        placeholder="Ask something..."
        disabled={busy || !activeModelId}
        on:keydown={(e) => {
          if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendChat();
          }
        }}
      ></textarea>
      <div class="chat-actions">
        <button on:click={sendChat} disabled={busy || !chatInput.trim() || !activeModelId}>
          {chatStreaming ? 'Streaming…' : 'Send'}
        </button>
        <button class="ghost" on:click={resetChat} disabled={busy || !activeModelId || chatHistory.length === 0}>
          Reset
        </button>
        <button class="ghost danger-text" on:click={wipeMemory} disabled={busy || !activeModelId}>
          Wipe Memory
        </button>
      </div>
    </div>
    <div class="metrics">
      {#if chatUsage}<span class="mono">Usage: {chatUsage}</span>{/if}
      {#if chatMetrics}<span class="mono"> | Metrics: {chatMetrics}</span>{/if}
    </div>
  </section>

  {#if apiError}
    <section class="card">
      <p class="error">API Error: {apiError}</p>
    </section>
  {/if}
</main>

<style>
  :global(:root) {
    --bg-deep: #0f172a;
    --bg-mid: #1e293b;
    --bg-card: #f8fafc;
    --ink: #0f172a;
    --muted: #64748b;
    --accent: #0ea5e9;
    --danger: #dc2626;
    font-family: 'Space Grotesk', sans-serif;
  }

  :global(body) {
    margin: 0;
    min-height: 100vh;
    color: #e2e8f0;
    background:
      radial-gradient(950px circle at 0% 0%, #0f766e 0%, transparent 42%),
      radial-gradient(1000px circle at 100% 0%, #1d4ed8 0%, transparent 48%),
      linear-gradient(140deg, var(--bg-deep), var(--bg-mid));
  }

  .shell {
    max-width: 900px;
    margin: 0 auto;
    padding: 2rem 1rem 3rem;
    display: flex;
    flex-direction: column;
    height: 100vh;
    box-sizing: border-box;
    gap: 1rem;
  }

  .hero {
    text-align: center;
  }
  .hero h1 {
    margin: 0;
    font-size: 2.5rem;
    color: #f8fafc;
  }

  .card {
    background: var(--bg-card);
    color: var(--ink);
    border-radius: 18px;
    padding: 1rem 1.2rem;
  }

  .model-controls .model-input-row {
    display: flex;
    gap: 0.5rem;
  }
  .model-controls input {
    flex: 1;
  }
  .active-status-row {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-top: 0.5rem;
  }
  .status-text {
    margin: 0;
    font-size: 0.9rem;
    color: var(--muted);
  }
  .badge-memory {
    background: #fef08a;
    color: #854d0e;
    padding: 0.2rem 0.6rem;
    border-radius: 999px;
    font-size: 0.75rem;
    font-weight: 600;
    box-shadow: 0 1px 2px rgba(0,0,0,0.05);
  }

  .chat-container {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 1rem;
    overflow: hidden;
  }

  .chat-history {
    flex: 1;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 1rem;
    padding-right: 0.5rem;
  }

  .message {
    padding: 0.8rem;
    border-radius: 12px;
  }
  .message.user {
    background: #e0f2fe;
    align-self: flex-end;
    max-width: 80%;
  }
  .message.assistant {
    background: #f1f5f9;
    align-self: flex-start;
    max-width: 95%;
  }
  .message pre {
    margin: 0.5rem 0 0 0;
    white-space: pre-wrap;
    font-family: inherit;
    font-size: 0.95rem;
    background: transparent;
    color: inherit;
    padding: 0;
  }
  .empty-state {
    color: var(--muted);
    text-align: center;
    margin: auto;
  }

  .chat-input-row {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
  }
  .chat-actions {
    display: flex;
    gap: 0.5rem;
    justify-content: flex-end;
  }

  .metrics {
    font-size: 0.8rem;
    color: var(--muted);
    text-align: right;
  }

  .mono { font-family: 'IBM Plex Mono', monospace; }

  input, textarea {
    font: inherit;
    border: 1px solid #cbd5e1;
    border-radius: 10px;
    padding: 0.5rem 0.6rem;
    width: 100%;
    box-sizing: border-box;
  }

  button {
    border: 0;
    border-radius: 999px;
    padding: 0.5rem 0.9rem;
    font-weight: 600;
    background: linear-gradient(90deg, var(--accent), #38bdf8);
    color: #082f49;
    cursor: pointer;
    white-space: nowrap;
  }

  button.ghost {
    background: #e2e8f0;
    color: #0f172a;
  }
  
  button.danger-text {
    color: var(--danger);
  }
  
  button.danger {
    background: var(--danger);
    color: white;
  }

  button:disabled {
    opacity: 0.6;
    cursor: not-allowed;
  }

  .error { color: var(--danger); font-weight: bold; }

  .cursor {
    animation: blink 0.7s step-end infinite;
    color: var(--accent);
  }

  @keyframes blink {
    0%, 100% { opacity: 1; }
    50% { opacity: 0; }
  }
</style>
