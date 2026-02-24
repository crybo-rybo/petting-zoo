<script lang="ts">
  import { onMount } from 'svelte';
  import { addMcpConnector, connectMcpConnector, disconnectMcpConnector, listMcpConnectors, removeMcpConnector } from './features/mcp/service';
  import type { McpConnectionStatus, McpConnector } from './shared/api/types';

  export let activeModelId: string | null = null;
  export let busy = false;

  // State
  let connectors: McpConnector[] = [];
  let connectionStatuses: Record<string, McpConnectionStatus> = {};
  
  let newId = '';
  let newCommand = '';
  let newArgs = '';
  
  let mcpError = '';
  let isLoading = false;
  let isMcpModalOpen = false;

  $: activeConnections = Object.values(connectionStatuses).filter(s => s.connected);
  $: totalTools = activeConnections.reduce((sum, s) => sum + s.discovered_tool_count, 0);
  $: summaryText = activeConnections.length > 0 
    ? `${activeConnections.length} server(s), ${totalTools} tool(s)` 
    : (connectors.length > 0 ? `${connectors.length} configured` : '0 connectors');

  async function loadConnectors() {
    try {
      mcpError = '';
      const data = await listMcpConnectors();
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

      const added = await addMcpConnector({
        id: newId.trim(),
        command: newCommand.trim(),
        args: argsArray,
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
      
      await removeMcpConnector(id);
      
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
      
      const status = await connectMcpConnector(id);
      
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
      
      await disconnectMcpConnector(id);
      
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

<button class="ghost tab-btn" on:click={() => isMcpModalOpen = true}>
  <span class="tab-icon">🔫</span>
  <div class="tab-info">
    <span class="tab-label">MCP Connectors</span>
    <span class="tab-summary">{summaryText}</span>
  </div>
</button>

{#if isMcpModalOpen}
  <!-- svelte-ignore a11y_click_events_have_key_events -->
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="modal-backdrop fade-in" on:click={() => isMcpModalOpen = false}>
    <div class="modal-content slide-up" on:click|stopPropagation>
      <div class="modal-header">
        <h3>MCP Tool Connectors</h3>
        <button class="ghost action-btn" on:click={() => isMcpModalOpen = false}>✕</button>
      </div>

      <div class="mcp-content">
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
                    <button class="ghost action-btn btn-small" on:click={() => removeConnector(connector.id)} disabled={busy || isLoading} aria-label="Remove connector" title="Remove connector">
                      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18"/><path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"/><path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"/></svg>
                    </button>
                  {/if}
                </div>
              </div>
            {/each}
          {/if}
        </div>
      </div>
    </div>
  </div>
{/if}

<style>
  .mcp-content {
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }

  .input-group {
    display: flex;
    flex-wrap: wrap;
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
    min-width: 0;
    font-family: inherit;
    font-weight: 600;
    background: #ffffff;
    border: 2px solid var(--border-main);
    border-radius: 4px;
    color: var(--text-main);
    transition: all 0.1s ease;
  }

  input:focus {
    outline: none;
    border-color: var(--accent-orange);
    box-shadow: 2px 2px 0px var(--accent-orange);
    transform: translate(-2px, -2px);
  }

  input:disabled {
    opacity: 0.6;
    cursor: not-allowed;
    background: #ebe6d8;
  }

  button {
    font-family: inherit;
    border: 2px solid var(--border-main);
    border-radius: 4px;
    font-weight: 700;
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
  
  .btn-add {
    height: 38px;
    padding: 0 1rem;
    font-size: 0.85rem;
  }
  
  .btn-small {
    padding: 0.3rem 0.6rem;
    font-size: 0.75rem;
    min-height: 24px;
    box-shadow: 2px 2px 0px var(--border-main);
  }
  
  .btn-small:active:not(:disabled) {
    transform: translate(2px, 2px);
    box-shadow: 0px 0px 0px var(--border-main);
  }

  .primary {
    background: var(--accent-orange);
    color: #fff;
  }

  .ghost {
    background: var(--bg-base);
    color: var(--text-main);
  }

  .ghost:not(:disabled):hover {
    background: #e6e1d1;
  }

  .primary.ghost {
    background: var(--bg-base);
    color: var(--accent-orange);
    border-color: var(--accent-orange);
    box-shadow: 2px 2px 0px var(--accent-orange);
  }

  .primary.ghost:not(:disabled):hover {
    background: #faeade;
  }

  .danger.ghost {
    color: var(--danger);
    border-color: var(--danger);
    box-shadow: 2px 2px 0px var(--danger);
  }
  
  .danger.ghost:not(:disabled):hover {
    background: var(--danger-muted);
  }
  
  .danger.ghost:active:not(:disabled) {
    transform: translate(2px, 2px);
    box-shadow: 0px 0px 0px var(--danger);
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

  .glow:hover:not(:disabled) {
    background: var(--accent-orange-hover);
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
    width: 8px;
  }
  .connectors-list::-webkit-scrollbar-track {
    background: transparent;
  }
  .connectors-list::-webkit-scrollbar-thumb {
    background: var(--border-main);
    border-radius: 0px;
  }
  .connectors-list::-webkit-scrollbar-thumb:hover {
    background: var(--accent-orange);
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
    background: #ffffff;
    border: 2px solid var(--border-main);
    box-shadow: 2px 2px 0px var(--border-main);
    padding: 0.5rem 0.75rem;
    border-radius: 4px;
    margin-bottom: 0.5rem;
  }

  .connector-info {
    display: flex;
    flex-direction: column;
    gap: 0.2rem;
    overflow: hidden;
  }

  .connector-id {
    font-weight: 700;
    font-size: 0.9rem;
    color: var(--text-main);
  }

  .connector-cmd {
    font-size: 0.75rem;
    color: var(--text-muted);
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
    background: #d4edda;
    color: #155724;
    border: 2px solid #155724;
    font-size: 0.7rem;
    padding: 0.15rem 0.4rem;
    border-radius: 4px;
    white-space: nowrap;
    font-weight: 700;
  }

  .mono {
    font-family: 'IBM Plex Mono', 'Fira Code', monospace;
  }
</style>
