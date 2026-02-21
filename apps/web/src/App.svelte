<script lang="ts">
  import { metricsSummary, usageSummary } from './lib/chat_format';

  type Health = {
    status: string;
    service: string;
    version: string;
    timestamp: string;
  };

  type ModelSummary = {
    id: string;
    display_name: string;
    path: string;
    status: 'available' | 'loading' | 'unavailable';
    context_size?: number;
  };

  let health: Health | null = null;
  let healthError = '';

  let models: ModelSummary[] = [];
  let activeModelId: string | null = null;
  let modelPath = '';
  let modelDisplayName = '';
  let selectedModelToLoad = '';

  let chatInput = '';
  let chatOutput = '';
  let chatError = '';
  let chatUsage = '';
  let chatMetrics = '';

  let apiError = '';
  let busy = false;

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
        // Fallback to status text when body is not JSON.
      }
      throw new Error(message);
    }
    return (await res.json()) as T;
  }

  async function loadHealth() {
    healthError = '';
    try {
      health = await requestJson<Health>('/healthz');
    } catch (e) {
      healthError = e instanceof Error ? e.message : 'unknown error';
      health = null;
    }
  }

  async function loadModels() {
    const body = await requestJson<{ models: ModelSummary[]; active_model_id: string | null }>(
      '/api/models'
    );
    models = body.models ?? [];
    activeModelId = body.active_model_id ?? null;
    if (!selectedModelToLoad && activeModelId) {
      selectedModelToLoad = activeModelId;
    }
  }

  async function initialize() {
    apiError = '';
    busy = true;
    try {
      await Promise.all([loadHealth(), loadModels()]);
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function registerModel() {
    apiError = '';
    chatError = '';
    busy = true;
    try {
      await requestJson<{ model: ModelSummary }>('/api/models/register', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          path: modelPath.trim(),
          display_name: modelDisplayName.trim() || undefined
        })
      });
      modelPath = '';
      modelDisplayName = '';
      await loadModels();
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function selectModel() {
    if (!selectedModelToLoad) {
      return;
    }
    apiError = '';
    chatError = '';
    busy = true;
    try {
      await requestJson<{ active_model: ModelSummary }>('/api/models/select', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ model_id: selectedModelToLoad })
      });
      await loadModels();
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function sendChat() {
    if (!chatInput.trim()) {
      return;
    }

    apiError = '';
    chatError = '';
    busy = true;
    try {
      const body = await requestJson<{
        text: string;
        usage: { prompt_tokens: number; completion_tokens: number; total_tokens: number };
        metrics: { latency_ms: number; time_to_first_token_ms: number; tokens_per_second: number };
      }>('/api/chat/complete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message: chatInput.trim() })
      });

      chatOutput = body.text ?? '';
      chatUsage = usageSummary(body.usage);
      chatMetrics = metricsSummary(body.metrics);
    } catch (e) {
      chatOutput = '';
      chatUsage = '';
      chatMetrics = '';
      chatError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function resetChat() {
    apiError = '';
    chatError = '';
    busy = true;
    try {
      await requestJson<{ status: string; model_id: string }>('/api/chat/reset', {
        method: 'POST'
      });
      chatOutput = '';
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
    <p class="eyebrow">MVP RESET</p>
    <h1>Petting Zoo</h1>
    <p class="lead">Load a local GGUF model with zoo-keeper and chat from the browser.</p>
    <div class="hero-actions">
      <button on:click={initialize} disabled={busy}>Reload</button>
      <button class="ghost" on:click={loadHealth} disabled={busy}>Health Only</button>
    </div>
  </section>

  <section class="card">
    <h2>Server Health</h2>
    {#if health}
      <dl>
        <div><dt>Status</dt><dd>{health.status}</dd></div>
        <div><dt>Service</dt><dd>{health.service}</dd></div>
        <div><dt>Version</dt><dd>{health.version}</dd></div>
        <div><dt>Timestamp</dt><dd>{health.timestamp}</dd></div>
      </dl>
    {:else if healthError}
      <p class="error">Failed to load health: {healthError}</p>
    {:else}
      <p>Loading...</p>
    {/if}
  </section>

  <section class="card">
    <h2>Model Runtime</h2>
    <p class="lead-in-card">Register a GGUF file path from this server, then load it as the active model.</p>
    <div class="grid">
      <label class="span-2">
        Model Path (server filesystem)
        <input bind:value={modelPath} placeholder="/absolute/path/to/model.gguf" />
      </label>
      <label class="span-2">
        Display Name (optional)
        <input bind:value={modelDisplayName} placeholder="Llama 3 8B Q4" />
      </label>
    </div>
    <div class="row-actions">
      <button on:click={registerModel} disabled={busy || !modelPath.trim()}>Register Model</button>
      <button class="ghost" on:click={loadModels} disabled={busy}>Refresh Models</button>
    </div>

    {#if models.length > 0}
      <div class="grid">
        <label class="span-2">
          Registered Models
          <select bind:value={selectedModelToLoad}>
            <option value="">Select model...</option>
            {#each models as model}
              <option value={model.id}>
                {model.display_name} ({model.status}) {model.id === activeModelId ? ' [active]' : ''}
              </option>
            {/each}
          </select>
        </label>
      </div>
      <div class="row-actions">
        <button on:click={selectModel} disabled={busy || !selectedModelToLoad}>Load Selected Model</button>
      </div>
      <ul>
        {#each models as model}
          <li>
            <span class="mono">{model.id}</span> - {model.display_name} - {model.status}
            <br />
            <span class="mono">{model.path}</span>
          </li>
        {/each}
      </ul>
    {:else}
      <p>No registered models yet.</p>
    {/if}
  </section>

  <section class="card">
    <h2>Chat</h2>
    <p class="lead-in-card">
      Active model: <span class="mono">{activeModelId ?? 'none loaded'}</span>
    </p>
    <label>
      Prompt
      <textarea bind:value={chatInput} rows="5" placeholder="Ask something..."></textarea>
    </label>
    <div class="row-actions">
      <button on:click={sendChat} disabled={busy || !chatInput.trim() || !activeModelId}>Send</button>
      <button class="ghost" on:click={resetChat} disabled={busy || !activeModelId}>Reset Chat History</button>
    </div>
    {#if chatError}
      <p class="error">Chat error: {chatError}</p>
    {/if}
    {#if chatOutput}
      <h3>Response</h3>
      <pre>{chatOutput}</pre>
      <p class="mono">Usage: {chatUsage}</p>
      <p class="mono">Metrics: {chatMetrics}</p>
    {/if}
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
    display: grid;
    gap: 1rem;
  }

  .hero {
    background: rgb(15 23 42 / 72%);
    border: 1px solid rgb(148 163 184 / 28%);
    border-radius: 18px;
    padding: 1.5rem;
  }

  .eyebrow {
    margin: 0;
    letter-spacing: 0.16em;
    font-size: 0.72rem;
    color: #bfdbfe;
    font-family: 'IBM Plex Mono', monospace;
  }

  h1 {
    margin: 0.35rem 0 0;
    font-size: clamp(2rem, 4vw, 3rem);
  }

  .lead {
    color: #d1d5db;
    max-width: 72ch;
  }

  .hero-actions,
  .row-actions {
    display: flex;
    gap: 0.6rem;
    flex-wrap: wrap;
  }

  .card {
    background: var(--bg-card);
    color: var(--ink);
    border-radius: 18px;
    padding: 1rem 1.2rem;
  }

  .lead-in-card {
    color: var(--muted);
    margin-top: 0;
  }

  h2,
  h3 {
    margin-top: 0;
  }

  dl {
    margin: 0;
    display: grid;
    gap: 0.55rem;
  }

  dl div {
    display: grid;
    grid-template-columns: 9rem 1fr;
    border-bottom: 1px dashed #cbd5e1;
    padding-bottom: 0.35rem;
  }

  dt,
  .mono {
    font-family: 'IBM Plex Mono', monospace;
  }

  dt {
    color: var(--muted);
  }

  .grid {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 0.7rem;
  }

  .span-2 {
    grid-column: span 2;
  }

  label {
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
    font-size: 0.9rem;
  }

  input,
  select,
  textarea {
    font: inherit;
    border: 1px solid #cbd5e1;
    border-radius: 10px;
    padding: 0.5rem 0.6rem;
  }

  pre {
    background: #0f172a;
    color: #f8fafc;
    border-radius: 10px;
    padding: 0.8rem;
    overflow-x: auto;
    white-space: pre-wrap;
  }

  button {
    border: 0;
    border-radius: 999px;
    padding: 0.5rem 0.9rem;
    font-weight: 600;
    background: linear-gradient(90deg, var(--accent), #38bdf8);
    color: #082f49;
    cursor: pointer;
  }

  button.ghost {
    background: #e2e8f0;
    color: #0f172a;
  }

  button:disabled {
    opacity: 0.6;
    cursor: not-allowed;
  }

  .error {
    color: var(--danger);
  }

  @media (max-width: 760px) {
    .grid {
      grid-template-columns: 1fr;
    }

    .span-2 {
      grid-column: span 1;
    }

    dl div {
      grid-template-columns: 1fr;
    }
  }
</style>
