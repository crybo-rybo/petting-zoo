<script lang="ts">
  import { parseArgs } from './lib/args';

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

  type Connector = {
    id: string;
    name: string;
    transport: 'stdio' | 'http_stream';
    status: 'connected' | 'degraded' | 'disconnected';
    protocol_version: string;
    command?: string;
    args?: string[];
    endpoint?: string;
    created_at: string;
    updated_at?: string | null;
  };

  type CatalogTemplate = {
    id: string;
    name: string;
    description?: string;
    transport: 'stdio' | 'http_stream';
    defaults?: {
      command?: string;
      args?: string[];
      endpoint?: string;
    };
  };

  type ValidationCheck = {
    name: string;
    ok: boolean;
    message?: string;
  };

  type ToolSummary = {
    name: string;
    description?: string;
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

  let templates: CatalogTemplate[] = [];
  let connectors: Connector[] = [];
  let toolsByConnector: Record<string, ToolSummary[]> = {};

  let apiError = '';
  let busy = false;

  let connectorName = '';
  let selectedTemplateId = '';
  let transport: 'stdio' | 'http_stream' = 'stdio';
  let protocolVersion = '2025-06-18';
  let command = '';
  let argsText = '';
  let endpoint = '';

  let validationChecks: ValidationCheck[] = [];
  let validationWarnings: string[] = [];
  let validationValid: boolean | null = null;

  function buildPayload() {
    return {
      name: connectorName.trim(),
      transport,
      protocol_version: protocolVersion.trim() || '2025-06-18',
      command: command.trim(),
      args: parseArgs(argsText),
      endpoint: endpoint.trim()
    };
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
        // No-op: fallback to status string.
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

  async function loadCatalog() {
    const body = await requestJson<{ templates: CatalogTemplate[] }>('/api/mcp/catalog');
    templates = body.templates ?? [];
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

  async function loadConnectors() {
    const body = await requestJson<{ connectors: Connector[] }>('/api/mcp/connectors');
    connectors = body.connectors ?? [];
  }

  async function initialize() {
    apiError = '';
    busy = true;
    try {
      await Promise.all([loadHealth(), loadModels(), loadCatalog(), loadConnectors()]);
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
      chatUsage = `prompt=${body.usage.prompt_tokens}, completion=${body.usage.completion_tokens}, total=${body.usage.total_tokens}`;
      chatMetrics = `latency=${body.metrics.latency_ms}ms, ttfb=${body.metrics.time_to_first_token_ms}ms, tps=${body.metrics.tokens_per_second.toFixed(2)}`;
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

  function applyTemplate() {
    const template = templates.find((entry) => entry.id === selectedTemplateId);
    if (!template) {
      return;
    }
    transport = template.transport;
    if (template.defaults?.command) {
      command = template.defaults.command;
    }
    if (template.defaults?.args) {
      argsText = template.defaults.args.join('\n');
    }
    if (template.defaults?.endpoint) {
      endpoint = template.defaults.endpoint;
    }
    if (!connectorName.trim()) {
      connectorName = template.name;
    }
  }

  async function validateConnector() {
    apiError = '';
    busy = true;
    try {
      const body = await requestJson<{
        valid: boolean;
        checks: ValidationCheck[];
        warnings?: string[];
      }>('/api/mcp/connectors/validate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(buildPayload())
      });
      validationValid = body.valid;
      validationChecks = body.checks ?? [];
      validationWarnings = body.warnings ?? [];
    } catch (e) {
      validationValid = null;
      validationChecks = [];
      validationWarnings = [];
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function createConnector() {
    apiError = '';
    busy = true;
    try {
      await requestJson<{ connector: Connector }>('/api/mcp/connectors', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(buildPayload())
      });
      validationValid = null;
      validationChecks = [];
      validationWarnings = [];
      await loadConnectors();
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function connectorAction(id: string, action: 'connect' | 'disconnect' | 'refresh-tools') {
    apiError = '';
    busy = true;
    try {
      await requestJson(`/api/mcp/connectors/${id}/${action}`, { method: 'POST' });
      await loadConnectors();
      if (action === 'connect' || action === 'refresh-tools') {
        await loadTools(id);
      }
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function deleteConnector(id: string) {
    apiError = '';
    busy = true;
    try {
      const res = await fetch(`/api/mcp/connectors/${id}`, { method: 'DELETE' });
      if (!res.ok) {
        throw new Error(`status ${res.status}`);
      }
      delete toolsByConnector[id];
      await loadConnectors();
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  async function loadTools(id: string) {
    apiError = '';
    busy = true;
    try {
      const body = await requestJson<{ tools: ToolSummary[] }>(`/api/mcp/connectors/${id}/tools`);
      toolsByConnector = { ...toolsByConnector, [id]: body.tools ?? [] };
    } catch (e) {
      apiError = e instanceof Error ? e.message : 'unknown error';
    } finally {
      busy = false;
    }
  }

  initialize();
</script>

<main class="shell">
  <section class="hero">
    <p class="eyebrow">MCP SUPPORT</p>
    <h1>Petting Zoo</h1>
    <p class="lead">Configure MCP connectors, validate settings, connect, and inspect discovered tools.</p>
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
    <p class="lead-in-card">Register a GGUF file path from this server, load it, and chat.</p>
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

  <section class="card">
    <h2>Create Connector</h2>
    <div class="grid">
      <label>
        Template
        <select bind:value={selectedTemplateId} on:change={applyTemplate}>
          <option value="">Custom</option>
          {#each templates as entry}
            <option value={entry.id}>{entry.name}</option>
          {/each}
        </select>
      </label>

      <label>
        Name
        <input bind:value={connectorName} placeholder="filesystem-local" />
      </label>

      <label>
        Transport
        <select bind:value={transport}>
          <option value="stdio">stdio</option>
          <option value="http_stream">http_stream</option>
        </select>
      </label>

      <label>
        Protocol Version
        <input bind:value={protocolVersion} placeholder="2025-06-18" />
      </label>

      <label>
        Command
        <input bind:value={command} placeholder="npx" />
      </label>

      <label>
        Endpoint
        <input bind:value={endpoint} placeholder="https://example.com/mcp" />
      </label>

      <label class="span-2">
        Args (one per line)
        <textarea bind:value={argsText} rows="5" placeholder="-y\n@modelcontextprotocol/server-filesystem\n."></textarea>
      </label>
    </div>

    <div class="row-actions">
      <button on:click={validateConnector} disabled={busy}>Validate</button>
      <button on:click={createConnector} disabled={busy}>Create</button>
    </div>

    {#if validationValid !== null}
      <div class={validationValid ? 'status-ok' : 'status-bad'}>
        Validation: {validationValid ? 'valid' : 'invalid'}
      </div>
      <ul>
        {#each validationChecks as check}
          <li class={check.ok ? 'ok' : 'bad'}>{check.name}: {check.message ?? (check.ok ? 'ok' : 'failed')}</li>
        {/each}
      </ul>
      {#if validationWarnings.length > 0}
        <p class="warning">Warnings: {validationWarnings.join(' | ')}</p>
      {/if}
    {/if}
  </section>

  <section class="card">
    <h2>Connectors</h2>
    {#if connectors.length === 0}
      <p>No connectors configured yet.</p>
    {:else}
      <div class="connector-list">
        {#each connectors as connector}
          <article>
            <header>
              <h3>{connector.name}</h3>
              <p class="mono">{connector.id}</p>
            </header>
            <p>
              <strong>Status:</strong> {connector.status} |
              <strong>Transport:</strong> {connector.transport}
            </p>
            <p>
              <strong>Command:</strong> {connector.command || '-'} |
              <strong>Endpoint:</strong> {connector.endpoint || '-'}
            </p>
            <div class="row-actions">
              <button on:click={() => connectorAction(connector.id, 'connect')} disabled={busy}>Connect</button>
              <button on:click={() => connectorAction(connector.id, 'disconnect')} disabled={busy}>Disconnect</button>
              <button on:click={() => connectorAction(connector.id, 'refresh-tools')} disabled={busy}>Refresh Tools</button>
              <button class="ghost" on:click={() => loadTools(connector.id)} disabled={busy}>Load Tools</button>
              <button class="danger" on:click={() => deleteConnector(connector.id)} disabled={busy}>Delete</button>
            </div>

            {#if toolsByConnector[connector.id]?.length}
              <ul>
                {#each toolsByConnector[connector.id] as tool}
                  <li><span class="mono">{tool.name}</span>{tool.description ? ` - ${tool.description}` : ''}</li>
                {/each}
              </ul>
            {/if}
          </article>
        {/each}
      </div>
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
    --bg-deep: #111827;
    --bg-mid: #1f2937;
    --bg-card: #f9fafb;
    --ink: #111827;
    --muted: #6b7280;
    --accent: #0ea5e9;
    --accent-2: #22c55e;
    --danger: #dc2626;
    --warning: #b45309;
    font-family: 'Space Grotesk', sans-serif;
  }

  :global(body) {
    margin: 0;
    min-height: 100vh;
    color: #f3f4f6;
    background:
      radial-gradient(900px circle at 0% 0%, #0f766e 0%, transparent 45%),
      radial-gradient(1000px circle at 100% 0%, #1d4ed8 0%, transparent 50%),
      linear-gradient(140deg, var(--bg-deep), var(--bg-mid));
  }

  .shell {
    max-width: 1024px;
    margin: 0 auto;
    padding: 2rem 1rem 3rem;
    display: grid;
    gap: 1rem;
  }

  .hero {
    background: rgb(17 24 39 / 72%);
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
    margin: 0.4rem 0 0;
    font-size: clamp(2rem, 4vw, 3rem);
  }

  .lead {
    color: #d1d5db;
    max-width: 72ch;
  }

  .hero-actions {
    display: flex;
    gap: 0.6rem;
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
    background: #111827;
    color: #f9fafb;
    border-radius: 10px;
    padding: 0.8rem;
    overflow-x: auto;
    white-space: pre-wrap;
  }

  .connector-list {
    display: grid;
    gap: 0.7rem;
  }

  article {
    border: 1px solid #d1d5db;
    border-radius: 12px;
    padding: 0.8rem;
    background: #fff;
  }

  article header {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: 0.5rem;
  }

  .row-actions {
    display: flex;
    gap: 0.5rem;
    flex-wrap: wrap;
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

  button.danger {
    background: #fee2e2;
    color: #7f1d1d;
  }

  button:disabled {
    opacity: 0.6;
    cursor: not-allowed;
  }

  .status-ok,
  .ok {
    color: #166534;
  }

  .status-bad,
  .bad,
  .error {
    color: var(--danger);
  }

  .warning {
    color: var(--warning);
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

    article header {
      flex-direction: column;
      align-items: flex-start;
    }
  }
</style>
