<script lang="ts">
  import { onMount, onDestroy } from 'svelte'
  import { fetchHealth } from '../lib/api/client'
  import type { HealthResponse } from '../lib/api/types'

  let health: HealthResponse | null = $state(null)
  let error: string | null = $state(null)
  let lastChecked: Date | null = $state(null)
  let intervalId: ReturnType<typeof setInterval> | null = null

  async function poll() {
    try {
      health = await fetchHealth()
      error = null
    } catch (e) {
      health = null
      error = e instanceof Error ? e.message : 'Unknown error'
    }
    lastChecked = new Date()
  }

  onMount(() => {
    poll()
    intervalId = setInterval(poll, 5000)
  })

  onDestroy(() => {
    if (intervalId) clearInterval(intervalId)
  })

  const statusText = $derived(
    error ? 'Unreachable' : health?.ready ? 'Ready' : 'Starting'
  )

  const statusClass = $derived(
    error ? 'red' : health?.ready ? 'green' : 'yellow'
  )
</script>

<div class="health-dashboard">
  <h1>Zoo Keeper Server</h1>

  <div class="status-row">
    <span class="dot {statusClass}"></span>
    <span class="status-text">{statusText}</span>
  </div>

  {#if health}
    <table class="details">
      <tbody>
        <tr><td>Version</td><td>{health.version}</td></tr>
        <tr><td>Model</td><td>{health.model_id}</td></tr>
        <tr><td>Status</td><td>{health.status}</td></tr>
      </tbody>
    </table>
  {/if}

  {#if error}
    <p class="error">{error}</p>
  {/if}

  {#if lastChecked}
    <p class="timestamp">Last checked: {lastChecked.toLocaleTimeString()}</p>
  {/if}
</div>

<style>
  .health-dashboard {
    max-width: 480px;
    margin: 2rem auto;
    font-family: system-ui, -apple-system, sans-serif;
  }

  h1 {
    font-size: 1.5rem;
    margin-bottom: 1rem;
  }

  .status-row {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    margin-bottom: 1.5rem;
  }

  .dot {
    width: 16px;
    height: 16px;
    border-radius: 50%;
    flex-shrink: 0;
  }

  .dot.green { background: #22c55e; }
  .dot.yellow { background: #eab308; }
  .dot.red { background: #ef4444; }

  .status-text {
    font-size: 1.25rem;
    font-weight: 600;
  }

  .details {
    width: 100%;
    border-collapse: collapse;
    font-family: ui-monospace, monospace;
    font-size: 0.875rem;
  }

  .details td {
    padding: 0.5rem 0.75rem;
    border-bottom: 1px solid #333;
  }

  .details td:first-child {
    color: #999;
    width: 100px;
  }

  .error {
    color: #ef4444;
    font-size: 0.875rem;
    margin-top: 1rem;
  }

  .timestamp {
    color: #666;
    font-size: 0.75rem;
    margin-top: 1rem;
  }
</style>
