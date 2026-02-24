<script lang="ts">
  import { onMount } from 'svelte';
  import { connectMcpConnector, disconnectMcpConnector, listMcpConnectors } from './features/mcp/service';
  import type { McpConnectionStatus, McpConnector } from './shared/api/types';

  export let activeModelId: string | null = null;
  export let busy = false;

  // State
  let connectors: McpConnector[] = [];
  let connectionStatuses: Record<string, McpConnectionStatus> = {};

  let mcpError = '';
  let isLoading = false;
  let isMcpModalOpen = false;

  $: activeConnections = Object.values(connectionStatuses).filter(s => s.connected);
  $: totalTools = activeConnections.reduce((sum, s) => sum + s.discovered_tool_count, 0);
  $: summaryText = activeConnections.length > 0
    ? `${activeConnections.length}/${connectors.length} enabled, ${totalTools} tool(s)`
    : (connectors.length > 0 ? `${connectors.length} server(s), none enabled` : 'No servers configured');

  async function loadConnectors() {
    try {
      mcpError = '';
      const data = await listMcpConnectors();
      connectors = data.connectors || [];
    } catch (e) {
      mcpError = e instanceof Error ? e.message : 'Failed to load MCP connectors';
    }
  }

  async function enableMcp(id: string) {
    if (!activeModelId) {
      mcpError = 'You must load a model before enabling an MCP server.';
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
      mcpError = e instanceof Error ? e.message : 'Failed to enable MCP server';
    } finally {
      isLoading = false;
    }
  }

  async function disableMcp(id: string) {
    try {
      isLoading = true;
      mcpError = '';

      await disconnectMcpConnector(id);

      connectionStatuses = {
        ...connectionStatuses,
        [id]: { server_id: id, connected: false, discovered_tool_count: 0 }
      };
    } catch (e) {
      mcpError = e instanceof Error ? e.message : 'Failed to disable MCP server';
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
        <h3>MCP Connectors</h3>
        <button class="ghost action-btn" on:click={() => isMcpModalOpen = false}>✕</button>
      </div>

      <div class="mcp-content">
        {#if mcpError}
          <p class="error-text slide-up">{mcpError}</p>
        {/if}

        <div class="connectors-list">
          {#if connectors.length === 0}
            <p class="empty-list">No MCP servers configured. Add entries to <span class="mono">config/app.json</span> and restart.</p>
          {:else}
            {#each connectors as connector (connector.id)}
              {@const status = connectionStatuses[connector.id]}
              {@const isConnected = status?.connected ?? false}
              <div class="connector-item {isConnected ? 'connected' : ''}">
                <div class="connector-info">
                  <span class="connector-id">{connector.id}</span>
                  {#if isConnected}
                    <span class="tool-count badge-success">
                      {status.discovered_tool_count} tool{status.discovered_tool_count !== 1 ? 's' : ''}
                    </span>
                  {:else}
                    <span class="status-badge badge-idle">disabled</span>
                  {/if}
                </div>
                <div class="connector-actions">
                  {#if isConnected}
                    <button class="danger ghost btn-small" on:click={() => disableMcp(connector.id)} disabled={busy || isLoading}>
                      Disable
                    </button>
                  {:else}
                    <button class="primary ghost btn-small" on:click={() => enableMcp(connector.id)} disabled={busy || isLoading || !activeModelId} title={!activeModelId ? 'Load a model first' : ''}>
                      Enable
                    </button>
                  {/if}
                </div>
              </div>
            {/each}
          {/if}
        </div>

        {#if connectors.length > 0 && !activeModelId}
          <p class="hint-text">Load a model to enable MCP servers.</p>
        {/if}
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

  .btn-small {
    padding: 0.3rem 0.8rem;
    font-size: 0.75rem;
    min-height: 28px;
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

  .error-text {
    color: #ef4444;
    font-size: 0.85rem;
    margin: 0;
  }

  .hint-text {
    margin: 0;
    font-size: 0.8rem;
    color: var(--text-muted);
    font-style: italic;
  }

  .connectors-list {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    max-height: 260px;
    overflow-y: auto;
    padding-right: 0.5rem;
  }

  /* Custom Scrollbar */
  .connectors-list::-webkit-scrollbar { width: 8px; }
  .connectors-list::-webkit-scrollbar-track { background: transparent; }
  .connectors-list::-webkit-scrollbar-thumb { background: var(--border-main); border-radius: 0px; }
  .connectors-list::-webkit-scrollbar-thumb:hover { background: var(--accent-orange); }

  .empty-list {
    margin: 0;
    font-size: 0.85rem;
    color: var(--text-muted);
    font-style: italic;
    opacity: 0.8;
    line-height: 1.5;
  }

  .connector-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    background: #ffffff;
    border: 2px solid var(--border-main);
    box-shadow: 2px 2px 0px var(--border-main);
    padding: 0.6rem 0.75rem;
    border-radius: 4px;
    transition: border-color 0.15s ease;
  }

  .connector-item.connected {
    border-color: #155724;
    box-shadow: 2px 2px 0px #155724;
  }

  .connector-info {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    overflow: hidden;
  }

  .connector-id {
    font-weight: 700;
    font-size: 0.9rem;
    color: var(--text-main);
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
    padding: 0.15rem 0.5rem;
    border-radius: 4px;
    white-space: nowrap;
    font-weight: 700;
  }

  .badge-idle {
    background: #f0ede4;
    color: var(--text-muted);
    border: 2px solid #c5c0b0;
    font-size: 0.7rem;
    padding: 0.15rem 0.5rem;
    border-radius: 4px;
    white-space: nowrap;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.05em;
  }

  .mono {
    font-family: 'IBM Plex Mono', 'Fira Code', monospace;
  }
</style>
