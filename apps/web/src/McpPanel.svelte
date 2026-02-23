<script lang="ts">
  import { onMount, createEventDispatcher } from 'svelte';
  import { slide } from 'svelte/transition';

  export let activeModelId: string | null = null;
  export let busy = false;

  const dispatch = createEventDispatcher();

  // Types
  type McpConnector = {
    id: string;
    command: string;
    args: string[];
  };

  type McpConnectionStatus = {
    server_id: string;
    connected: boolean;
    discovered_tool_count: number;
  };

  // State
  let connectors: McpConnector[] = [];
  let connectionStatuses: Record<string, McpConnectionStatus> = {};
  
  let newId = '';
  let newCommand = '';
  let newArgs = '';
  
  let mcpError = '';
  let isLoading = false;
  let isMcpPanelOpen = false;

  $: activeConnections = Object.values(connectionStatuses).filter(s => s.connected);
  $: totalTools = activeConnections.reduce((sum, s) => sum + s.discovered_tool_count, 0);
  $: summaryText = activeConnections.length > 0 
    ? `${activeConnections.length} server(s), ${totalTools} tool(s)` 
    : (connectors.length > 0 ? `${connectors.length} configured` : '0 connectors');

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

  async function loadConnectors() {
    try {
      mcpError = '';
      const data = await requestJson<{ connectors: McpConnector[] }>('/api/mcp/connectors');
      connectors = data.connectors || [];
    } catch (e) {
      mcpError = e instanceof Error ? e.message : 'Failed to load MCP connectors';
    }
  }

  async function addConnector() {
    if (!newId.trim() || !newCommand.trim()) return;
    try {
      isLoading = true;
      mcpError = '';
      
      const argsArray = newArgs
        .split(' ')
        .map(a => a.trim())
        .filter(a => a.length > 0);

      const added = await requestJson<McpConnector>('/api/mcp/connectors', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          id: newId.trim(),
          command: newCommand.trim(),
          args: argsArray
        })
      });

      connectors = [...connectors, added];
      newId = '';
      newCommand = '';
      newArgs = '';
    } catch (e) {
      mcpError = e instanceof Error ? e.message : 'Failed to add connector';
    } finally {
      isLoading = false;
    }
  }

  async function removeConnector(id: string) {
    if (!confirm(`Are you sure you want to remove connector ${id}?`)) return;
    try {
      isLoading = true;
      mcpError = '';
      
      await requestJson(`/api/mcp/connectors/${encodeURIComponent(id)}`, {
        method: 'DELETE'
      });
      
      connectors = connectors.filter(c => c.id !== id);
      const newStatuses = { ...connectionStatuses };
      delete newStatuses[id];
      connectionStatuses = newStatuses;
    } catch (e) {
      mcpError = e instanceof Error ? e.message : 'Failed to remove connector';
    } finally {
      isLoading = false;
    }
  }

  async function connectMcp(id: string) {
    if (!activeModelId) {
      mcpError = 'You must load a model before connecting an MCP server.';
      return;
    }
    
    try {
      isLoading = true;
      mcpError = '';
      
      const status = await requestJson<McpConnectionStatus>(`/api/mcp/connectors/${encodeURIComponent(id)}/connect`, {
        method: 'POST'
      });
      
      connectionStatuses = {
        ...connectionStatuses,
        [id]: status
      };
    } catch (e) {
      mcpError = e instanceof Error ? e.message : 'Failed to connect MCP server';
    } finally {
      isLoading = false;
    }
  }

  async function disconnectMcp(id: string) {
    try {
      isLoading = true;
      mcpError = '';
      
      await requestJson(`/api/mcp/connectors/${encodeURIComponent(id)}/disconnect`, {
        method: 'POST'
      });
      
      connectionStatuses = {
        ...connectionStatuses,
        [id]: { server_id: id, connected: false, discovered_tool_count: 0 }
      };
    } catch (e) {
      mcpError = e instanceof Error ? e.message : 'Failed to disconnect MCP server';
    } finally {
      isLoading = false;
    }
  }
  
  // When activeModelId changes to null, all MCP servers are implicitly disconnected
  $: {
    if (!activeModelId && Object.keys(connectionStatuses).length > 0) {
      connectionStatuses = {};
    }
  }

  onMount(() => {
    loadConnectors();
  });
</script>

<section class="card glass mcp-panel slide-up">
  <!-- svelte-ignore a11y_click_events_have_key_events -->
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="mcp-header" on:click={() => isMcpPanelOpen = !isMcpPanelOpen}>
    <div class="header-left">
      <div class="badge-mcp" title="Model Context Protocol Configuration">
        <span class="badge-icon">🔌</span>
      </div>
      <div class="header-title">
        <h3 class:active={isMcpPanelOpen}>MCP Tool Connectors</h3>
        {#if !isMcpPanelOpen}
          <span class="header-summary fade-in">{summaryText}</span>
        {/if}
      </div>
    </div>
    <div class="chevron" class:open={isMcpPanelOpen}>
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 9l6 6 6-6"/></svg>
    </div>
  </div>

  {#if isMcpPanelOpen}
    <div class="mcp-content" transition:slide|local={{ duration: 250 }}>
      <div class="add-connector-form">
        <div class="input-group">
          <input 
            bind:value={newId} 
            placeholder="Identifier (e.g. 'fs')" 
            disabled={busy || isLoading} 
            class="small-input"
          />
          <input 
            bind:value={newCommand} 
            placeholder="Command (e.g. 'npx')" 
            disabled={busy || isLoading} 
            class="small-input cmd-input"
          />
          <input 
            bind:value={newArgs} 
            placeholder="Args (space sep) '-y @...'" 
            disabled={busy || isLoading} 
            class="small-input"
          />
          <button class="primary glow btn-add" on:click={addConnector} disabled={busy || isLoading || !newId.trim() || !newCommand.trim()}>
            Add
          </button>
        </div>
      </div>

      {#if mcpError}
        <p class="error-text slide-up">{mcpError}</p>
      {/if}

      <div class="connectors-list">
        {#if connectors.length === 0}
          <p class="empty-list">No MCP connectors configured.</p>
        {:else}
          {#each connectors as connector (connector.id)}
            <div class="connector-item">
              <div class="connector-info">
                <span class="connector-id">{connector.id}</span>
                <span class="connector-cmd mono">
                  {connector.command} {connector.args.join(' ')}
                </span>
              </div>
              <div class="connector-actions">
                {#if connectionStatuses[connector.id]?.connected}
                  <span class="tool-count badge-success">
                    {connectionStatuses[connector.id].discovered_tool_count} tools
                  </span>
                  <button class="danger ghost btn-small" on:click={() => disconnectMcp(connector.id)} disabled={busy || isLoading}>
                    Disconnect
                  </button>
                {:else}
                  <button class="primary ghost btn-small" on:click={() => connectMcp(connector.id)} disabled={busy || isLoading || !activeModelId}>
                    Connect
                  </button>
                  <button class="ghost action-btn btn-small" on:click={() => removeConnector(connector.id)} disabled={busy || isLoading}>
                    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18"/><path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"/><path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"/></svg>
                  </button>
                {/if}
              </div>
            </div>
          {/each}
        {/if}
      </div>
    </div>
  {/if}
</section>

<style>
  .mcp-panel {
    display: flex;
    flex-direction: column;
    gap: 1rem;
    padding: 1.25rem 1.5rem;
  }

  .mcp-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    cursor: pointer;
    user-select: none;
    margin: -0.5rem -0.5rem 0;
    padding: 0.5rem;
    border-radius: 12px;
    transition: background-color 0.2s ease;
  }
  
  .mcp-header:hover {
    background-color: rgba(255, 255, 255, 0.03);
  }

  .mcp-header:hover .header-title h3 {
    color: var(--text-main);
  }

  .header-left {
    display: flex;
    align-items: center;
    gap: 0.75rem;
  }

  .header-title {
    display: flex;
    flex-direction: column;
    gap: 0.1rem;
  }

  .header-title h3 {
    margin: 0;
    font-size: 1.1rem;
    font-weight: 600;
    color: var(--text-muted);
    transition: color 0.2s ease;
  }
  
  .header-title h3.active {
    color: var(--text-main);
  }

  .header-summary {
    font-size: 0.85rem;
    color: var(--text-muted);
    opacity: 0.8;
  }

  .chevron {
    color: var(--text-muted);
    transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .chevron.open {
    transform: rotate(180deg);
  }

  .badge-mcp {
    background: rgba(14, 165, 233, 0.15);
    border: 1px solid rgba(14, 165, 233, 0.3);
    padding: 0.3rem;
    border-radius: 999px;
    display: flex;
    align-items: center;
    justify-content: center;
    width: 2rem;
    height: 2rem;
    box-sizing: border-box;
  }

  .mcp-content {
    margin-top: 1.25rem;
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }

  .input-group {
    display: flex;
    gap: 0.5rem;
    align-items: stretch;
  }

  .small-input {
    padding: 0.5rem 0.75rem;
    font-size: 0.85rem;
    height: 38px;
  }

  .cmd-input {
    max-width: 120px;
  }

  input {
    flex: 1;
    font-family: inherit;
    background: rgba(0, 0, 0, 0.2);
    border: 1px solid var(--border-glass);
    border-radius: 8px;
    color: var(--text-main);
    transition: all 0.2s ease;
  }

  input:focus {
    outline: none;
    border-color: rgba(14, 165, 233, 0.5);
    background: rgba(0, 0, 0, 0.4);
    box-shadow: 0 0 0 2px rgba(14, 165, 233, 0.1);
  }

  input:disabled {
    opacity: 0.55;
    cursor: default;
    filter: saturate(0.65);
  }

  button {
    font-family: inherit;
    border: 1px solid transparent;
    border-radius: 6px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.2s ease;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 0.4rem;
  }
  
  .btn-add {
    height: 38px;
    padding: 0 1rem;
    font-size: 0.85rem;
  }
  
  .btn-small {
    padding: 0.3rem 0.6rem;
    font-size: 0.75rem;
    min-height: 24px;
  }

  .primary {
    background: linear-gradient(135deg, #148bc7, #356dd1);
    color: white;
  }

  .ghost {
    background: rgba(255, 255, 255, 0.02);
    border-color: rgba(148, 163, 184, 0.24);
    color: var(--text-main);
  }

  .primary.ghost {
    color: #38bdf8;
    border-color: rgba(56, 189, 248, 0.3);
  }

  .primary.ghost:hover:not(:disabled) {
    background: rgba(56, 189, 248, 0.1);
    border-color: rgba(56, 189, 248, 0.5);
  }

  .danger.ghost {
    color: #ef4444;
    border-color: rgba(239, 68, 68, 0.3);
  }
  
  .danger.ghost:hover:not(:disabled) {
    background: rgba(239, 68, 68, 0.1);
    border-color: rgba(239, 68, 68, 0.5);
  }

  button:disabled {
    opacity: 0.48;
    cursor: default;
    transform: none !important;
    box-shadow: none !important;
  }

  .glow:hover:not(:disabled) {
    box-shadow: 0 4px 12px rgba(14, 165, 233, 0.25);
    transform: translateY(-0.5px);
  }

  .error-text {
    color: #ef4444;
    font-size: 0.85rem;
    margin: 0;
  }

  .connectors-list {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    max-height: 200px;
    overflow-y: auto;
    padding-right: 0.5rem;
  }

  /* Custom Scrollbar */
  .connectors-list::-webkit-scrollbar {
    width: 6px;
  }
  .connectors-list::-webkit-scrollbar-track {
    background: transparent;
  }
  .connectors-list::-webkit-scrollbar-thumb {
    background: rgba(255, 255, 255, 0.1);
    border-radius: 10px;
  }
  .connectors-list::-webkit-scrollbar-thumb:hover {
    background: rgba(255, 255, 255, 0.2);
  }

  .empty-list {
    margin: 0;
    font-size: 0.85rem;
    color: var(--text-muted);
    font-style: italic;
    opacity: 0.8;
  }

  .connector-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    background: rgba(0, 0, 0, 0.2);
    border: 1px solid rgba(255, 255, 255, 0.05);
    padding: 0.5rem 0.75rem;
    border-radius: 8px;
  }

  .connector-info {
    display: flex;
    flex-direction: column;
    gap: 0.2rem;
    overflow: hidden;
  }

  .connector-id {
    font-weight: 600;
    font-size: 0.9rem;
    color: #e2e8f0;
  }

  .connector-cmd {
    font-size: 0.75rem;
    color: #94a3b8;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .connector-actions {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    flex-shrink: 0;
  }

  .badge-success {
    background: rgba(16, 185, 129, 0.15);
    color: #34d399;
    border: 1px solid rgba(16, 185, 129, 0.3);
    font-size: 0.7rem;
    padding: 0.15rem 0.4rem;
    border-radius: 4px;
    white-space: nowrap;
  }

  .mono {
    font-family: 'IBM Plex Mono', 'Fira Code', monospace;
  }
</style>
