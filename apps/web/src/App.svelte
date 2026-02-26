<script lang="ts">
  import { metricsSummary, usageSummary } from './lib/chat_format';
  import { renderAssistantMarkdown } from './lib/markdown_render';
  import { tick } from 'svelte';

  import McpPanel from './McpPanel.svelte';
  import type { ModelSummary } from './shared/api/types';
  import { listModels, selectModel, unloadModel as unloadSelectedModel } from './features/models/service';
  import { clearMemory, consumeSseStream, openChatStream, resetChat as resetChatSession } from './features/chat/stream';

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

  // Context size options
  const CONTEXT_SIZE_OPTIONS = [
    { value: 512, label: '512' },
    { value: 1024, label: '1K' },
    { value: 2048, label: '2K' },
    { value: 4096, label: '4K' },
    { value: 8192, label: '8K' },
  ];

  function formatFileSize(bytes: number): string {
    if (bytes >= 1e9) return (bytes / 1e9).toFixed(1) + ' GB';
    if (bytes >= 1e6) return (bytes / 1e6).toFixed(0) + ' MB';
    return (bytes / 1e3).toFixed(0) + ' KB';
  }

  // State
  let availableModels: ModelSummary[] = [];
  let selectedModelId = '';
  let selectedContextSize = 2048;
  let activeModelId: string | null = null;
  let busy = false;
  let apiError = '';
  let isModelModalOpen = false;

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

  async function initialize() {
    apiError = '';
    busy = true;
    try {
      const body = await listModels();
      availableModels = body.models ?? [];
      activeModelId = body.active_model_id ?? null;
      if (availableModels.length > 0 && !selectedModelId) {
        selectedModelId = availableModels[0].id;
      }
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function loadAndSelectModel() {
    if (!selectedModelId) return;
    apiError = '';
    chatError = '';
    busy = true;
    try {
      const selResp = await selectModel(selectedModelId, selectedContextSize);
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
      await unloadSelectedModel();
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
      const responseBody = await openChatStream(latestMessage, abortController.signal);

      await consumeSseStream(responseBody, async (event) => {
        if (event.type === 'token' && event.content !== undefined) {
          chatHistory[chatHistory.length - 1].content += event.content;
          chatHistory = [...chatHistory];

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
      });
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
      await resetChatSession();
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
      await clearMemory();
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

  <!-- Top Tabs -->
  <div class="top-tabs">
    <button class="ghost tab-btn" on:click={() => isModelModalOpen = true}>
      <span class="tab-icon">🤠</span>
      <div class="tab-info">
        <span class="tab-label">Model Configuration</span>
        <span class="tab-summary">
          {#if activeModelId}
            Active: <span class="mono accent-text" title={activeModelId}>{availableModels.find(m => m.id === activeModelId)?.display_name ?? activeModelId}</span>
          {:else}
            No model loaded
          {/if}
        </span>
      </div>
    </button>
    
    <McpPanel {activeModelId} {busy} />
  </div>

  <!-- Model Configuration Modal -->
  {#if isModelModalOpen}
    <!-- svelte-ignore a11y_click_events_have_key_events -->
    <!-- svelte-ignore a11y_no_static_element_interactions -->
    <div class="modal-backdrop fade-in" on:click={() => isModelModalOpen = false}>
      <div class="modal-content slide-up" on:click|stopPropagation>
        <div class="modal-header">
          <h3>Model Configuration</h3>
          <button class="ghost action-btn" on:click={() => isModelModalOpen = false}>✕</button>
        </div>
        
        <div class="panel-content">
          {#if activeModelId}
            <div class="active-status-row fade-in">
              <p class="status-text">Active: <span class="mono accent-text">{availableModels.find(m => m.id === activeModelId)?.display_name ?? activeModelId}</span></p>
              <div class="badge-memory" title="Long-term context database is active">
                <span class="badge-icon">🧠</span>
                <span class="badge-text">Memory Active</span>
              </div>
            </div>
            <button class="danger ghost" on:click={unloadModel} disabled={busy}>Unload Model</button>
          {:else}
            <div class="model-input-row">
              {#if availableModels.length === 0}
                <p class="control-hint">No models found in configured discovery paths.</p>
              {:else}
                <select bind:value={selectedModelId} disabled={busy} class="model-select">
                  {#each availableModels as model (model.id)}
                    <option value={model.id} disabled={model.status === 'unavailable'}>
                      {model.display_name ?? model.id}{model.status === 'unavailable' ? ' (unavailable)' : ''}{model.file_size_bytes ? ` (${formatFileSize(model.file_size_bytes)})` : ''}
                    </option>
                  {/each}
                </select>
                <button class="primary glow load-model-btn" on:click={loadAndSelectModel} disabled={busy || !selectedModelId}>
                  <span>Load</span>
                </button>
              {/if}
            </div>
            {#if availableModels.length > 0}
              <div class="context-size-row">
                <label class="context-label" for="ctx-size">Context Window:</label>
                <select id="ctx-size" bind:value={selectedContextSize} disabled={busy} class="context-select">
                  {#each CONTEXT_SIZE_OPTIONS as opt (opt.value)}
                    <option value={opt.value}>{opt.label} tokens</option>
                  {/each}
                </select>
              </div>
              <p class="control-hint">Larger context uses more memory. Reduce if model loading fails with OOM.</p>
            {/if}
          {/if}
        </div>
      </div>
    </div>
  {/if}

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
    --bg-base: #fdfbf7; /* Desert Creme */
    --bg-card: #ffffff;
    --border-main: #233d31; /* Dark Muted Green */
    
    --text-main: #233d31;
    --text-muted: #5a7366;
    
    --accent-orange: #d97746; /* Burnt Orange */
    --accent-orange-hover: #e08b5e;
    
    --msg-user: #d97746;
    --msg-assistant: #ffffff;
    
    --danger: #cf4f4f;
    --danger-muted: rgba(207, 79, 79, 0.15);
    font-family: 'Courier New', Courier, monospace;
    font-weight: 600;
  }

  :global(body) {
    margin: 0;
    min-height: 100vh;
    color: var(--text-main);
    background-color: var(--bg-base);
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
    border-radius: 8px;
    border: 2px solid var(--border-main);
    background: var(--bg-card);
    box-shadow: 4px 4px 0px var(--border-main);
  }
  
  .hero-content {
    display: flex;
    align-items: center;
    gap: 1rem;
  }
  
  .logo {
    width: 64px;
    height: 64px;
    image-rendering: pixelated; /* good for pixel art */
  }

  .hero h1 {
    margin: 0;
    font-size: 2.2rem;
    font-weight: 800;
    letter-spacing: -0.05em;
    color: var(--text-main);
    text-transform: uppercase;
  }

  /* ---- FLAT CARD ---- */
  :global(.glass) {
    background: var(--bg-card);
    border: 2px solid var(--border-main);
    border-radius: 8px;
    box-shadow: 4px 4px 0px var(--border-main);
  }

  :global(.card) {
    padding: 1.5rem;
  }

  /* ---- TOP TABS ---- */
  .top-tabs {
    display: flex;
    gap: 1rem;
    width: 100%;
  }

  :global(.tab-btn) {
    flex: 1;
    display: flex;
    align-items: center;
    justify-content: flex-start;
    gap: 1rem;
    padding: 0.75rem 1rem;
    border-radius: 4px;
    border: 2px solid var(--border-main);
    background: var(--bg-card);
    box-shadow: 4px 4px 0px var(--border-main);
    cursor: pointer;
    transition: all 0.1s ease;
    text-align: left;
  }

  :global(.tab-btn:hover) {
    transform: translate(-2px, -2px);
    box-shadow: 6px 6px 0px var(--border-main);
    background: #e6e1d1;
  }
  
  :global(.tab-btn:active) {
    transform: translate(4px, 4px);
    box-shadow: 0px 0px 0px var(--border-main);
  }

  :global(.tab-icon) {
    font-size: 1.8rem;
  }

  :global(.tab-info) {
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }

  :global(.tab-label) {
    font-size: 1rem;
    font-weight: 800;
    color: var(--text-main);
  }

  :global(.tab-summary) {
    font-size: 0.8rem;
    color: var(--text-muted);
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  /* ---- MODAL OVERLAY ---- */
  :global(.modal-backdrop) {
    position: fixed;
    top: 0;
    left: 0;
    width: 100vw;
    height: 100vh;
    background: rgba(35, 61, 49, 0.4);
    backdrop-filter: blur(4px);
    -webkit-backdrop-filter: blur(4px);
    z-index: 1000;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 1rem;
    box-sizing: border-box;
  }

  :global(.modal-content) {
    width: 100%;
    max-width: 600px;
    padding: 2rem;
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    gap: 1.5rem;
    background: var(--bg-card);
    border: 2px solid var(--border-main);
    border-radius: 4px;
    box-shadow: 6px 6px 0px var(--border-main);
  }
  
  :global(.modal-header) {
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-bottom: 2px solid var(--border-main);
    padding-bottom: 1rem;
  }
  
  :global(.modal-header h3) {
    margin: 0;
    font-size: 1.4rem;
    font-weight: 800;
    text-transform: uppercase;
  }
  
  :global(.close-btn) {
    min-height: 2rem;
    padding: 0.2rem 0.6rem;
    font-size: 1.2rem;
  }

  /* ---- INPUTS & BUTTONS ---- */
  .model-input-row {
    display: flex;
    gap: 0.75rem;
    align-items: stretch;
  }

  select.model-select {
    font-family: inherit;
    font-size: 0.95rem;
    font-weight: 600;
    background: #ffffff;
    border: 2px solid var(--border-main);
    border-radius: 4px;
    padding: 0.75rem 1rem;
    color: var(--text-main);
    flex: 1;
    box-sizing: border-box;
    cursor: pointer;
    transition: all 0.1s ease;
    appearance: auto;
  }

  select.model-select:focus {
    outline: none;
    border-color: var(--accent-orange);
    box-shadow: 2px 2px 0px var(--accent-orange);
    transform: translate(-2px, -2px);
  }

  select.model-select:disabled {
    opacity: 0.6;
    cursor: not-allowed;
    background: #ebe6d8;
  }

  .context-size-row {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    margin-top: 0.75rem;
  }

  .context-label {
    font-size: 0.85rem;
    font-weight: 700;
    color: var(--text-muted);
    white-space: nowrap;
  }

  select.context-select {
    font-family: inherit;
    font-size: 0.85rem;
    font-weight: 600;
    background: #ffffff;
    border: 2px solid var(--border-main);
    border-radius: 4px;
    padding: 0.4rem 0.6rem;
    color: var(--text-main);
    cursor: pointer;
    transition: all 0.1s ease;
    appearance: auto;
  }

  select.context-select:focus {
    outline: none;
    border-color: var(--accent-orange);
    box-shadow: 2px 2px 0px var(--accent-orange);
  }

  select.context-select:disabled {
    opacity: 0.6;
    cursor: not-allowed;
    background: #ebe6d8;
  }

  textarea {
    font-family: inherit;
    font-size: 0.95rem;
    font-weight: 600;
    background: #ffffff;
    border: 2px solid var(--border-main);
    border-radius: 4px;
    padding: 0.75rem 1rem;
    color: var(--text-main);
    width: 100%;
    box-sizing: border-box;
    transition: all 0.1s ease;
  }
  
  textarea:focus {
    outline: none;
    border-color: var(--accent-orange);
    box-shadow: 2px 2px 0px var(--accent-orange);
    transform: translate(-2px, -2px);
  }
  
  textarea:disabled {
    opacity: 0.6;
    cursor: not-allowed;
    background: #ebe6d8;
  }

  button {
    font-family: inherit;
    border: 2px solid var(--border-main);
    border-radius: 4px;
    padding: 0.4rem 0.7rem;
    min-height: 2.1rem;
    font-weight: 700;
    font-size: 0.88rem;
    line-height: 1.1;
    letter-spacing: 0.05em;
    cursor: pointer;
    transition: all 0.1s ease;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 0.4rem;
    text-transform: uppercase;
    box-shadow: 3px 3px 0px var(--border-main);
  }
  
  button:active:not(:disabled) {
    transform: translate(3px, 3px);
    box-shadow: 0px 0px 0px var(--border-main);
  }

  button.primary {
    background: var(--accent-orange);
    color: #fff;
  }

  button.load-model-btn {
    min-width: 8rem;
    padding-inline: 0.7rem;
    white-space: nowrap;
    flex-shrink: 0;
  }
  
  button.glow:not(:disabled):hover {
    background: var(--accent-orange-hover);
  }

  button.ghost {
    background: var(--bg-base);
    color: var(--text-main);
  }
  
  button.ghost:not(:disabled):hover {
    background: #e6e1d1;
  }
  
  button.danger.ghost {
    color: var(--danger);
    border-color: var(--danger);
    box-shadow: 3px 3px 0px var(--danger);
  }
  
  button.danger.ghost:not(:disabled):hover {
    background: var(--danger-muted);
  }
  
  button.danger.ghost:active:not(:disabled) {
    transform: translate(3px, 3px);
    box-shadow: 0px 0px 0px var(--danger);
  }

  button.danger-text {
    color: var(--danger);
  }

  button:disabled {
    opacity: 0.48;
    cursor: not-allowed;
    transform: none !important;
    box-shadow: none !important;
    background: #ebe6d8 !important;
    border-color: #a4ad9c !important;
    color: #79857d !important;
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
    background: var(--accent-orange);
    color: #fff;
    border: 2px solid var(--border-main);
    padding: 0.3rem;
    border-radius: 4px;
    font-size: 0.75rem;
    font-weight: 700;
    letter-spacing: 0.05em;
    display: flex;
    align-items: center;
    gap: 0;
    overflow: hidden;
    white-space: nowrap;
    transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
    box-shadow: 2px 2px 0px var(--border-main);
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
    transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
  }
  
  .badge-memory:hover {
    padding: 0.3rem 0.8rem 0.3rem 0.3rem;
    gap: 0.3rem;
    transform: translate(-1px, -1px);
    box-shadow: 3px 3px 0px var(--border-main);
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
    background: var(--bg-card);
    color: var(--text-main);
    border: 2px solid var(--border-main);
    border-radius: 4px;
    min-height: 1.8rem;
    padding: 0.3rem 0.7rem;
    font-size: 0.8rem;
    font-weight: 700;
    box-shadow: 4px 4px 0px var(--border-main);
    transition: all 0.1s ease;
  }
  .jump-btn:hover {
    background: var(--accent-orange);
    color: #fff;
    transform: translateX(-50%) translate(-2px, -2px);
    box-shadow: 6px 6px 0px var(--border-main);
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
    width: 8px;
  }
  .chat-history::-webkit-scrollbar-track {
    background: transparent;
  }
  .chat-history::-webkit-scrollbar-thumb {
    background: var(--border-main);
    border-radius: 0px;
  }
  .chat-history::-webkit-scrollbar-thumb:hover {
    background: var(--accent-orange);
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
    border-radius: 4px;
    line-height: 1.6;
    border: 2px solid var(--border-main);
    box-shadow: 4px 4px 0px var(--border-main);
  }
  
  .message-header {
    font-size: 0.8rem;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
    margin-bottom: 0.5rem;
    font-weight: 700;
  }

  .message.user {
    background: var(--msg-user);
    color: #fff;
    align-self: flex-end;
    max-width: 80%;
  }
  
  .message.user .message-header {
    color: #fff;
    opacity: 0.8;
  }

  .message.assistant {
    background: var(--msg-assistant);
    align-self: flex-start;
    max-width: 90%;
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
    min-height: 1.8rem;
    padding: 0.25rem 0.6rem;
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

  :global(.fade-in) { animation: fadeIn 0.4s ease-out forwards; }
  :global(.slide-up) { animation: slideUp 0.3s ease-out forwards; }
  :global(.pulse-badge) { animation: pulseGlow 2s infinite; }

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
