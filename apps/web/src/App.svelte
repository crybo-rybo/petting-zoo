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
  <header class="hero glass">
    <div class="hero-content">
      <img src="/logo.png" alt="Petting Zoo Logo" class="logo" />
      <h1>Petting Zoo</h1>
    </div>
  </header>

  <!-- Model loading / unloading -->
  <section class="card model-controls glass">
    <div class="model-input-row">
      <input 
        bind:value={modelPath} 
        placeholder="/absolute/path/to/model.gguf" 
        disabled={busy || activeModelId !== null} 
      />
      {#if !activeModelId}
        <button class="primary glow" on:click={loadAndSelectModel} disabled={busy || !modelPath.trim()}>Load Model</button>
      {:else}
        <button class="danger ghost" on:click={unloadModel} disabled={busy}>Unload</button>
      {/if}
    </div>
    {#if activeModelId}
      <div class="active-status-row fade-in">
        <p class="status-text">Active model: <span class="mono accent-text">{activeModelId}</span></p>
        <div class="badge-memory pulse-badge" title="Long-term context database is active">🧠 Memory Active</div>
      </div>
    {/if}
  </section>

  <section class="card chat-container glass">
    <div class="chat-history">
      {#if chatHistory.length === 0}
        <div class="empty-state fade-in">
          <div class="empty-icon">✨</div>
          <p>No messages yet. Send a message to start.</p>
        </div>
      {:else}
        {#each chatHistory as msg, i}
          <div class="message {msg.role} slide-up">
            <div class="message-header">
              <strong>{msg.role === 'user' ? 'You' : 'Zoo Model'}</strong>
            </div>
            <pre>{msg.content}{#if chatStreaming && i === chatHistory.length - 1}<span class="cursor"></span>{/if}</pre>
          </div>
        {/each}
        {#if busy && chatHistory.length > 0 && chatHistory[chatHistory.length - 1].role === 'user'}
           <div class="message assistant slide-up typing">
             <div class="typing-indicator">
               <span></span><span></span><span></span>
             </div>
           </div>
        {/if}
      {/if}
    </div>
    
    {#if chatError}
      <p class="error slide-up">Error: {chatError}</p>
    {/if}
    
    <div class="chat-input-wrapper">
      <div class="chat-input-row">
        <textarea 
          bind:value={chatInput} 
          rows="2" 
          placeholder="Type your message..."
          disabled={busy || !activeModelId}
          on:keydown={(e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
              e.preventDefault();
              sendChat();
            }
          }}
        ></textarea>
        <div class="chat-actions">
          <button class="primary glow" on:click={sendChat} disabled={busy || !chatInput.trim() || !activeModelId}>
            {chatStreaming ? 'Streaming…' : 'Send'}
          </button>
        </div>
      </div>
      <div class="secondary-actions">
        <button class="ghost action-btn" on:click={resetChat} disabled={busy || !activeModelId || chatHistory.length === 0}>
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8"/><path d="M3 3v5h5"/></svg>
          Reset Session
        </button>
        <button class="ghost danger-text action-btn" on:click={wipeMemory} disabled={busy || !activeModelId}>
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18"/><path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"/><path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"/></svg>
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
    <section class="card glass error-card slide-up">
      <p class="error">API Error: {apiError}</p>
    </section>
  {/if}
</main>

<style>
  :global(:root) {
    --bg-base: #020617; /* Very dark blue/black */
    --bg-glass: rgba(15, 23, 42, 0.6);
    --bg-glass-hover: rgba(15, 23, 42, 0.8);
    --border-glass: rgba(255, 255, 255, 0.08);
    
    --text-main: #f8fafc;
    --text-muted: #94a3b8;
    
    --accent-glow: #0ea5e9;
    --accent-magenta: #d946ef;
    
    --msg-user: rgba(14, 165, 233, 0.15);
    --msg-assistant: rgba(255, 255, 255, 0.03);
    
    --danger: #ef4444;
    --danger-muted: rgba(239, 68, 68, 0.15);
    font-family: 'Inter', system-ui, -apple-system, sans-serif;
  }

  :global(body) {
    margin: 0;
    min-height: 100vh;
    color: var(--text-main);
    background-color: var(--bg-base);
    background-image: 
      radial-gradient(circle at 15% 50%, rgba(14, 165, 233, 0.15), transparent 25%),
      radial-gradient(circle at 85% 30%, rgba(217, 70, 239, 0.15), transparent 25%);
    background-attachment: fixed;
  }

  /* ---- LAYOUT ---- */
  .shell {
    max-width: 900px;
    margin: 0 auto;
    padding: 2rem 1rem 3rem;
    display: flex;
    flex-direction: column;
    height: 100vh;
    box-sizing: border-box;
    gap: 1.5rem;
  }

  .hero {
    display: flex;
    justify-content: center;
    padding: 1rem;
    border-radius: 24px;
    border: 1px solid var(--border-glass);
    box-shadow: 0 8px 32px rgba(0,0,0,0.2);
  }
  
  .hero-content {
    display: flex;
    align-items: center;
    gap: 1rem;
  }
  
  .logo {
    width: 48px;
    height: 48px;
    border-radius: 12px;
    box-shadow: 0 0 15px rgba(14, 165, 233, 0.4);
  }

  .hero h1 {
    margin: 0;
    font-size: 2.2rem;
    font-weight: 800;
    letter-spacing: -0.05em;
    background: linear-gradient(135deg, #38bdf8, #d946ef);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
  }

  /* ---- GLASSMORPHISM ---- */
  .glass {
    background: var(--bg-glass);
    backdrop-filter: blur(16px);
    -webkit-backdrop-filter: blur(16px);
    border: 1px solid var(--border-glass);
    border-radius: 20px;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
  }

  .card {
    padding: 1.5rem;
  }

  /* ---- INPUTS & BUTTONS ---- */
  .model-input-row {
    display: flex;
    gap: 0.75rem;
  }

  input, textarea {
    font-family: inherit;
    font-size: 0.95rem;
    background: rgba(0, 0, 0, 0.2);
    border: 1px solid var(--border-glass);
    border-radius: 12px;
    padding: 0.75rem 1rem;
    color: var(--text-main);
    width: 100%;
    box-sizing: border-box;
    transition: all 0.2s ease;
  }
  
  input:focus, textarea:focus {
    outline: none;
    border-color: rgba(14, 165, 233, 0.5);
    background: rgba(0, 0, 0, 0.4);
    box-shadow: 0 0 0 2px rgba(14, 165, 233, 0.1);
  }
  
  input:disabled, textarea:disabled {
    opacity: 0.6;
    cursor: not-allowed;
  }

  button {
    font-family: inherit;
    border: 0;
    border-radius: 12px;
    padding: 0.75rem 1.2rem;
    font-weight: 600;
    font-size: 0.95rem;
    cursor: pointer;
    transition: all 0.2s ease;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 0.5rem;
  }

  button.primary {
    background: linear-gradient(135deg, #0ea5e9, #3b82f6);
    color: white;
  }
  
  button.glow:not(:disabled):hover {
    box-shadow: 0 0 15px rgba(14, 165, 233, 0.4);
    transform: translateY(-1px);
  }

  button.ghost {
    background: transparent;
    color: var(--text-muted);
  }
  
  button.ghost:not(:disabled):hover {
    background: rgba(255, 255, 255, 0.05);
    color: var(--text-main);
  }
  
  button.danger.ghost {
    color: var(--danger);
  }
  
  button.danger.ghost:not(:disabled):hover {
    background: var(--danger-muted);
  }
  
  button.danger-text {
    color: var(--danger);
  }

  button:disabled {
    opacity: 0.5;
    cursor: not-allowed;
    transform: none !important;
    box-shadow: none !important;
  }

  /* ---- STATUS ROW ---- */
  .active-status-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-top: 1rem;
    padding-top: 1rem;
    border-top: 1px solid var(--border-glass);
  }
  
  .status-text {
    margin: 0;
    font-size: 0.9rem;
    color: var(--text-muted);
  }
  
  .accent-text {
    color: #38bdf8;
  }

  .badge-memory {
    background: rgba(217, 70, 239, 0.15);
    color: #f0abfc;
    border: 1px solid rgba(217, 70, 239, 0.3);
    padding: 0.3rem 0.8rem;
    border-radius: 999px;
    font-size: 0.75rem;
    font-weight: 600;
    letter-spacing: 0.02em;
  }

  /* ---- CHAT AREA ---- */
  .chat-container {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 1.5rem;
    overflow: hidden;
  }

  .chat-history {
    flex: 1;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 1.2rem;
    padding-right: 0.5rem;
    scroll-behavior: smooth;
  }
  
  /* Custom Scrollbar */
  .chat-history::-webkit-scrollbar {
    width: 6px;
  }
  .chat-history::-webkit-scrollbar-track {
    background: transparent;
  }
  .chat-history::-webkit-scrollbar-thumb {
    background: rgba(255, 255, 255, 0.1);
    border-radius: 10px;
  }
  .chat-history::-webkit-scrollbar-thumb:hover {
    background: rgba(255, 255, 255, 0.2);
  }

  .empty-state {
    margin: auto;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 1rem;
    color: var(--text-muted);
    font-size: 1.1rem;
  }
  .empty-icon {
    font-size: 3rem;
    opacity: 0.5;
  }

  .message {
    padding: 1rem 1.2rem;
    border-radius: 16px;
    line-height: 1.6;
    border: 1px solid var(--border-glass);
  }
  
  .message-header {
    font-size: 0.8rem;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
    margin-bottom: 0.5rem;
  }

  .message.user {
    background: var(--msg-user);
    align-self: flex-end;
    max-width: 80%;
    border-bottom-right-radius: 4px;
    border-color: rgba(14, 165, 233, 0.2);
  }
  
  .message.user .message-header {
    color: #7dd3fc;
  }

  .message.assistant {
    background: var(--msg-assistant);
    align-self: flex-start;
    max-width: 90%;
    border-bottom-left-radius: 4px;
  }

  .message pre {
    margin: 0;
    white-space: pre-wrap;
    font-family: inherit;
    font-size: 0.95rem;
    color: var(--text-main);
  }

  /* ---- INPUT AREA ---- */
  .chat-input-wrapper {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .chat-input-row {
    display: flex;
    gap: 0.75rem;
    align-items: flex-end;
  }
  
  .chat-input-row textarea {
    flex: 1;
    resize: none;
    min-height: 50px;
  }

  .secondary-actions {
    display: flex;
    justify-content: space-between;
    padding-top: 0.5rem;
    border-top: 1px solid var(--border-glass);
  }
  
  .action-btn {
    padding: 0.4rem 0.8rem;
    font-size: 0.85rem;
  }

  /* ---- TYPING INDICATOR ---- */
  .typing-indicator {
    display: flex;
    gap: 5px;
    padding: 0.5rem 0;
  }
  
  .typing-indicator span {
    width: 6px;
    height: 6px;
    background-color: var(--text-muted);
    border-radius: 50%;
    animation: bounce 1.4s infinite ease-in-out both;
  }
  
  .typing-indicator span:nth-child(1) { animation-delay: -0.32s; }
  .typing-indicator span:nth-child(2) { animation-delay: -0.16s; }

  /* ---- ANIMATIONS & UTILS ---- */
  .mono { font-family: 'IBM Plex Mono', 'Fira Code', monospace; }
  
  .error { color: var(--danger); font-weight: 500; }
  .error-card { border-color: rgba(239, 68, 68, 0.3); }

  .metrics {
    font-size: 0.75rem;
    color: var(--text-muted);
    text-align: right;
    opacity: 0.8;
  }

  .cursor {
    display: inline-block;
    width: 8px;
    height: 1.2em;
    background-color: #38bdf8;
    vertical-align: middle;
    margin-left: 2px;
    animation: blink 1s step-end infinite;
  }

  .fade-in { animation: fadeIn 0.4s ease-out forwards; }
  .slide-up { animation: slideUp 0.3s ease-out forwards; }
  .pulse-badge { animation: pulseGlow 2s infinite; }

  @keyframes blink {
    0%, 100% { opacity: 1; }
    50% { opacity: 0; }
  }
  
  @keyframes bounce {
    0%, 80%, 100% { transform: scale(0); }
    40% { transform: scale(1); }
  }

  @keyframes fadeIn {
    from { opacity: 0; }
    to { opacity: 1; }
  }
  
  @keyframes slideUp {
    from { opacity: 0; transform: translateY(10px); }
    to { opacity: 1; transform: translateY(0); }
  }
  
  @keyframes pulseGlow {
    0% { box-shadow: 0 0 0 0 rgba(217, 70, 239, 0.4); }
    70% { box-shadow: 0 0 0 6px rgba(217, 70, 239, 0); }
    100% { box-shadow: 0 0 0 0 rgba(217, 70, 239, 0); }
  }
</style>
