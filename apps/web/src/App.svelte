<script lang="ts">
  import { metricsSummary, usageSummary } from './lib/chat_format';
  import { renderAssistantMarkdown } from './lib/markdown_render';
  import { tick } from 'svelte';
  
  import DOMPurify from 'dompurify';
  import 'highlight.js/styles/atom-one-dark.css';

  function handleCopy(event: MouseEvent) {
    const target = event.target as HTMLElement;
    if (target && target.classList.contains('copy-btn')) {
      const wrapper = target.closest('.code-block-wrapper');
      if (wrapper) {
        const codeElement = wrapper.querySelector('code');
        if (codeElement) {
          navigator.clipboard.writeText(codeElement.innerText);
          const originalText = target.innerText;
          target.innerText = 'Copied!';
          setTimeout(() => {
            if (target.innerText === 'Copied!') target.innerText = originalText;
          }, 2000);
        }
      }
    }
  }

  function autoResize(node: HTMLTextAreaElement) {
    function resize() {
      node.style.height = 'auto';
      node.style.height = node.scrollHeight + 'px';
    }
    node.addEventListener('input', resize);
    tick().then(resize);
    return {
      destroy() {
        node.removeEventListener('input', resize);
      }
    };
  }

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
  let abortController: AbortController | null = null;

  // Auto-scroll State
  let chatContainer: HTMLElement;
  let autoScrollEnabled = true;
  let isResponseTopOffscreen = false;

  function checkResponsePosition() {
    if (!chatContainer || chatHistory.length === 0) {
      isResponseTopOffscreen = false;
      return;
    }
    const lastMsgIndex = chatHistory.length - 1;
    if (chatHistory[lastMsgIndex].role !== 'assistant') {
      isResponseTopOffscreen = false;
      return;
    }
    const el = document.getElementById(`message-${lastMsgIndex}`);
    if (el) {
      const elRect = el.getBoundingClientRect();
      const containerRect = chatContainer.getBoundingClientRect();
      isResponseTopOffscreen = elRect.top < containerRect.top - 10;
    } else {
      isResponseTopOffscreen = false;
    }
  }

  function handleScroll() {
    if (!chatContainer) return;
    const isAtBottom = chatContainer.scrollHeight - chatContainer.scrollTop - chatContainer.clientHeight < 50;
    autoScrollEnabled = isAtBottom;
    checkResponsePosition();
  }

  function jumpToActiveResponse() {
    const lastMsgIndex = chatHistory.length - 1;
    if (lastMsgIndex >= 0 && chatHistory[lastMsgIndex].role === 'assistant') {
      document.getElementById(`message-${lastMsgIndex}`)?.scrollIntoView({ behavior: 'smooth', block: 'start' });
      autoScrollEnabled = false;
    }
  }

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
    autoScrollEnabled = true;
    isResponseTopOffscreen = false;

    chatHistory = [...chatHistory, { role: 'assistant', content: '' }];
    
    abortController = new AbortController();

    try {
      const latestMessage = chatHistory[chatHistory.length - 2].content;

      const resp = await fetch('/api/chat/stream', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message: latestMessage }),
        signal: abortController.signal
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
             
             if (autoScrollEnabled) {
               await tick();
               if (chatContainer) chatContainer.scrollTop = chatContainer.scrollHeight;
             }
             checkResponsePosition();
          } else if (event.type === 'done') {
            if (event.usage) chatUsage = usageSummary(event.usage);
            if (event.metrics) chatMetrics = metricsSummary(event.metrics);
          } else if (event.type === 'error') {
            chatError = event.message ?? event.code ?? 'error';
          }
        }
      }
    } catch (e) {
      if (e instanceof Error && e.name === 'AbortError') {
        chatError = '';
        chatHistory = [...chatHistory, { role: 'assistant', content: '\n\n*(Generation stopped)*' }];
      } else {
        chatError = e instanceof Error ? e.message : 'unknown error';
      }
    } finally {
      busy = false;
      chatStreaming = false;
      abortController = null;
    }
  }
  
  function stopGeneration() {
    if (abortController) {
      abortController.abort();
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
        <button class="primary glow load-model-btn" on:click={loadAndSelectModel} disabled={busy || !modelPath.trim()}>
          <span>Load Model</span>
        </button>
      {:else}
        <button class="danger ghost" on:click={unloadModel} disabled={busy}>Unload</button>
      {/if}
    </div>
    {#if !activeModelId}
      <p class="control-hint">Load a model to enable chat controls.</p>
    {/if}
    {#if activeModelId}
      <div class="active-status-row fade-in">
        <p class="status-text">Active model: <span class="mono accent-text">{activeModelId}</span></p>
        <div class="badge-memory" title="Long-term context database is active">
          <span class="badge-icon">🧠</span>
          <span class="badge-text">Memory Active</span>
        </div>
      </div>
    {/if}
  </section>

  <section class="card chat-container glass">
    {#if isResponseTopOffscreen}
      <button class="jump-btn fade-in" on:click={jumpToActiveResponse} title="Jump to top of recent response">
        ↑ Top of Response
      </button>
    {/if}
    <div class="chat-history" bind:this={chatContainer} on:scroll={handleScroll}>
      {#if chatHistory.length === 0}
        <div class="empty-state fade-in">
          <div class="empty-icon">✨</div>
          <p>No messages yet. Send a message to start.</p>
        </div>
      {:else}
        {#each chatHistory as msg, i}
          <div class="message {msg.role} slide-up" id="message-{i}">
            <div class="message-header">
              <strong>{msg.role === 'user' ? 'You' : 'Zoo Model'}</strong>
            </div>
            <!-- svelte-ignore a11y_no_static_element_interactions -->
            <!-- svelte-ignore a11y_click_events_have_key_events -->
            {#if msg.role === 'assistant'}
              <div class="markdown-body {chatStreaming && i === chatHistory.length - 1 ? 'streaming' : ''}" on:click={handleCopy}>
                {@html DOMPurify.sanitize(renderAssistantMarkdown(msg.content))}
              </div>
            {:else}
              <pre>{msg.content}</pre>
            {/if}
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
          rows="1" 
          use:autoResize
          style="max-height: 200px; overflow-y: auto;"
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
          {#if chatStreaming}
            <button class="danger ghost" on:click={stopGeneration}>Stop</button>
          {:else}
            <button class="primary glow" on:click={sendChat} disabled={busy || !chatInput.trim() || !activeModelId}>
              Send
            </button>
          {/if}
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
    opacity: 0.55;
    cursor: default;
    filter: saturate(0.65);
  }

  button {
    font-family: inherit;
    border: 1px solid transparent;
    border-radius: 10px;
    padding: 0.55rem 0.95rem;
    min-height: 2.35rem;
    font-weight: 600;
    font-size: 0.88rem;
    line-height: 1.1;
    letter-spacing: 0.01em;
    cursor: pointer;
    transition: all 0.2s ease;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 0.4rem;
    white-space: nowrap;
  }

  button.primary {
    background: linear-gradient(135deg, #148bc7, #356dd1);
    border-color: rgba(125, 211, 252, 0.35);
    color: white;
    box-shadow: 0 1px 0 rgba(255, 255, 255, 0.08) inset;
  }

  button.load-model-btn {
    min-width: 8.2rem;
    padding-inline: 0.85rem;
    white-space: nowrap;
    flex-shrink: 0;
    box-shadow: 0 0 0 1px rgba(14, 165, 233, 0.1) inset;
  }
  
  button.glow:not(:disabled):hover {
    box-shadow: 0 8px 18px rgba(14, 165, 233, 0.22);
    transform: translateY(-0.5px);
  }

  button.ghost {
    background: rgba(255, 255, 255, 0.02);
    border-color: rgba(148, 163, 184, 0.24);
    color: var(--text-muted);
  }
  
  button.ghost:not(:disabled):hover {
    background: rgba(255, 255, 255, 0.07);
    border-color: rgba(148, 163, 184, 0.38);
    color: var(--text-main);
  }
  
  button.danger.ghost {
    border-color: rgba(239, 68, 68, 0.35);
    color: var(--danger);
  }
  
  button.danger.ghost:not(:disabled):hover {
    background: rgba(239, 68, 68, 0.12);
  }
  
  button.danger-text {
    color: var(--danger);
  }

  button:disabled {
    opacity: 0.48;
    cursor: default;
    color: rgba(203, 213, 225, 0.65);
    border-color: rgba(148, 163, 184, 0.18);
    background: rgba(51, 65, 85, 0.35);
    filter: saturate(0.6);
    transform: none !important;
    box-shadow: none !important;
  }

  .control-hint {
    margin: 0.6rem 0 0;
    font-size: 0.78rem;
    color: rgba(148, 163, 184, 0.9);
    letter-spacing: 0.01em;
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
    padding: 0.3rem;
    border-radius: 999px;
    font-size: 0.75rem;
    font-weight: 600;
    letter-spacing: 0.02em;
    display: flex;
    align-items: center;
    gap: 0;
    overflow: hidden;
    white-space: nowrap;
    transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    box-shadow: 0 0 0 0 rgba(217, 70, 239, 0.4);
    animation: pulseGlow 2s infinite;
    cursor: default;
  }
  
  .badge-icon {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 1.5rem;
    height: 1.5rem;
    font-size: 1rem;
  }
  
  .badge-text {
    max-width: 0;
    opacity: 0;
    transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
  }
  
  .badge-memory:hover {
    padding: 0.3rem 0.8rem 0.3rem 0.3rem;
    gap: 0.3rem;
    background: rgba(217, 70, 239, 0.25);
    border-color: rgba(217, 70, 239, 0.5);
    animation: none;
    box-shadow: 0 0 10px rgba(217, 70, 239, 0.3);
  }
  
  .badge-memory:hover .badge-text {
    max-width: 100px;
    opacity: 1;
  }

  /* ---- CHAT AREA ---- */
  .chat-container {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 1.5rem;
    overflow: hidden;
    position: relative;
  }

  .jump-btn {
    position: absolute;
    top: 1rem;
    left: 50%;
    transform: translateX(-50%);
    z-index: 10;
    background: rgba(15, 23, 42, 0.9);
    color: #38bdf8;
    border: 1px solid rgba(56, 189, 248, 0.3);
    border-radius: 999px;
    min-height: 2rem;
    padding: 0.4rem 0.85rem;
    font-size: 0.8rem;
    font-weight: 600;
    box-shadow: 0 4px 12px rgba(0,0,0,0.3);
    backdrop-filter: blur(8px);
    transition: all 0.2s ease;
  }
  .jump-btn:hover {
    background: rgba(15, 23, 42, 1);
    color: #7dd3fc;
    border-color: rgba(56, 189, 248, 0.6);
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
    min-height: 2rem;
    padding: 0.35rem 0.7rem;
    font-size: 0.8rem;
  }

  /* ---- MARKDOWN ---- */
  :global(.markdown-body) {
    font-size: 0.95rem;
    line-height: 1.6;
    color: var(--text-main);
  }
  
  :global(.markdown-body p) { margin-top: 0; margin-bottom: 1rem; }
  :global(.markdown-body p:last-child) { margin-bottom: 0; }
  
  :global(.markdown-body code:not(.hljs)) {
    background: rgba(0, 0, 0, 0.3);
    padding: 0.2rem 0.4rem;
    border-radius: 4px;
    font-family: 'IBM Plex Mono', 'Fira Code', monospace;
    font-size: 0.9em;
  }
  
  :global(.markdown-body pre) {
    background: #1e1e1e;
    padding: 0;
    border-radius: 8px;
    overflow: hidden;
    margin: 1rem 0;
  }
  
  :global(.markdown-body pre code) {
    background: transparent;
    padding: 1rem;
    display: block;
    overflow-x: auto;
  }
  
  :global(.code-block-wrapper) {
    position: relative;
    margin: 1rem 0;
    border-radius: 8px;
    overflow: hidden;
    border: 1px solid var(--border-glass);
  }
  
  :global(.code-block-wrapper pre) {
    margin: 0;
    border-radius: 0;
  }
  
  :global(.code-header) {
    display: flex;
    justify-content: space-between;
    align-items: center;
    background: rgba(0, 0, 0, 0.4);
    padding: 0.4rem 1rem;
    font-size: 0.8rem;
  }
  
  :global(.lang-label) {
    color: var(--text-muted);
    font-family: monospace;
    text-transform: uppercase;
  }
  
  :global(.copy-btn) {
    background: rgba(255, 255, 255, 0.1);
    border: none;
    color: var(--text-main);
    padding: 0.2rem 0.6rem;
    border-radius: 4px;
    font-size: 0.75rem;
    cursor: pointer;
  }
  
  :global(.copy-btn:hover) {
    background: rgba(255, 255, 255, 0.2);
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
  
  /* When streaming markdown, forcefully append a pseudo-element cursor to the very last child element */
  :global(.markdown-body.streaming > *:last-child::after) {
    content: '';
    display: inline-block;
    width: 8px;
    height: 1.2em;
    background-color: #38bdf8;
    vertical-align: middle;
    margin-left: 4px;
    animation: blink 1s step-end infinite;
  }
  
  /* To ensure the cursor appears INSIDE a codeblock if that's the last element */
  :global(.markdown-body.streaming > .code-block-wrapper:last-child pre code::after) {
    content: '';
    display: inline-block;
    width: 8px;
    height: 1.2em;
    background-color: #38bdf8;
    vertical-align: middle;
    margin-left: 4px;
    animation: blink 1s step-end infinite;
  }
  :global(.markdown-body.streaming > .code-block-wrapper:last-child::after) {
    display: none; /* Hide the outer level cursor if inner level is active */
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
