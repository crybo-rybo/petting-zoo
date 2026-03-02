<script lang="ts">
  import { createEventDispatcher } from 'svelte';

  import Modal from './Modal.svelte';
  import type { ModelSummary } from '../shared/api/types';

  export let open = false;
  export let activeModelId: string | null = null;
  export let availableModels: ModelSummary[] = [];
  export let selectedModelId = '';
  export let selectedContextSize = 2048;
  export let busy = false;

  const dispatch = createEventDispatcher<{
    close: void;
    load: void;
    unload: void;
    selectedModelIdChange: string;
    selectedContextSizeChange: number;
  }>();

  const contextSizeOptions = [
    { value: 512, label: '512' },
    { value: 1024, label: '1K' },
    { value: 2048, label: '2K' },
    { value: 4096, label: '4K' },
    { value: 8192, label: '8K' }
  ];

  function formatFileSize(bytes: number): string {
    if (bytes >= 1e9) return (bytes / 1e9).toFixed(1) + ' GB';
    if (bytes >= 1e6) return (bytes / 1e6).toFixed(0) + ' MB';
    return (bytes / 1e3).toFixed(0) + ' KB';
  }
</script>

<Modal open={open} title="Model Configuration" closeLabel="Close model configuration" on:close={() => dispatch('close')}>
  <div class="panel-content">
    {#if activeModelId}
      <div class="active-status-row fade-in">
        <p class="status-text">Active: <span class="mono accent-text">{availableModels.find(m => m.id === activeModelId)?.display_name ?? activeModelId}</span></p>
        <div class="badge-memory" title="Long-term context database is active">
          <span class="badge-icon">🧠</span>
          <span class="badge-text">Memory Active</span>
        </div>
      </div>
      <button class="danger ghost" type="button" on:click={() => dispatch('unload')} disabled={busy}>Unload Model</button>
    {:else}
      <div class="model-input-row">
        {#if availableModels.length === 0}
          <p class="control-hint">No models found in configured discovery paths.</p>
        {:else}
          <select
            value={selectedModelId}
            disabled={busy}
            class="model-select"
            on:change={(event) => dispatch('selectedModelIdChange', (event.currentTarget as HTMLSelectElement).value)}
          >
            {#each availableModels as model (model.id)}
              <option value={model.id} disabled={model.status === 'unavailable'}>
                {model.display_name ?? model.id}{model.status === 'unavailable' ? ' (unavailable)' : ''}{model.file_size_bytes ? ` (${formatFileSize(model.file_size_bytes)})` : ''}
              </option>
            {/each}
          </select>
          <button class="primary glow load-model-btn" type="button" on:click={() => dispatch('load')} disabled={busy || !selectedModelId}>
            <span>Load</span>
          </button>
        {/if}
      </div>
      {#if availableModels.length > 0}
        <div class="context-size-row">
          <label class="context-label" for="ctx-size">Context Window:</label>
          <select
            id="ctx-size"
            value={selectedContextSize}
            disabled={busy}
            class="context-select"
            on:change={(event) => dispatch('selectedContextSizeChange', Number((event.currentTarget as HTMLSelectElement).value))}
          >
            {#each contextSizeOptions as opt (opt.value)}
              <option value={opt.value}>{opt.label} tokens</option>
            {/each}
          </select>
        </div>
        <p class="control-hint">Larger context uses more memory. Reduce if model loading fails with OOM.</p>
      {/if}
    {/if}
  </div>
</Modal>
