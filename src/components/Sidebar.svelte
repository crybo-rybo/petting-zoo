<script lang="ts">
  import { onMount, onDestroy } from 'svelte'
  import { fetchHealth } from '../lib/api/client'

  interface Props {
    activeView: 'health' | 'chat'
    onNavigate: (view: 'health' | 'chat') => void
    onHealthStatus: (ready: boolean) => void
  }

  let { activeView, onNavigate, onHealthStatus }: Props = $props()

  let healthStatus: 'green' | 'yellow' | 'red' = $state('yellow')
  let intervalId: ReturnType<typeof setInterval> | null = null

  async function pollHealth() {
    try {
      const health = await fetchHealth()
      healthStatus = health.ready ? 'green' : 'yellow'
      onHealthStatus(health.ready)
    } catch {
      healthStatus = 'red'
      onHealthStatus(false)
    }
  }

  onMount(() => {
    pollHealth()
    intervalId = setInterval(pollHealth, 5000)
  })

  onDestroy(() => {
    if (intervalId) clearInterval(intervalId)
  })
</script>

<nav class="sidebar">
  <button
    class="nav-btn"
    class:active={activeView === 'health'}
    onclick={() => onNavigate('health')}
    title="Server Health"
  >
    <span class="health-dot {healthStatus}"></span>
  </button>

  <button
    class="nav-btn"
    class:active={activeView === 'chat'}
    onclick={() => onNavigate('chat')}
    title="Chat"
  >
    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>
    </svg>
  </button>

  <button
    class="nav-btn disabled"
    title="Metrics (coming soon)"
    disabled
  >
    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M18 20V10M12 20V4M6 20v-6"/>
    </svg>
  </button>
</nav>

<style>
  .sidebar {
    width: 56px;
    background: #1a1a1a;
    border-right: 1px solid #333;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 12px 0;
    gap: 8px;
    flex-shrink: 0;
  }

  .nav-btn {
    width: 40px;
    height: 40px;
    border: none;
    border-radius: 8px;
    background: transparent;
    color: #999;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    transition: background 0.15s, color 0.15s;
  }

  .nav-btn:hover:not(.disabled) {
    background: #333;
    color: #e5e5e5;
  }

  .nav-btn.active {
    background: #333;
    color: #e5e5e5;
  }

  .nav-btn.disabled {
    color: #555;
    cursor: not-allowed;
  }

  .health-dot {
    width: 12px;
    height: 12px;
    border-radius: 50%;
  }

  .health-dot.green { background: #22c55e; }
  .health-dot.yellow { background: #eab308; }
  .health-dot.red { background: #ef4444; }
</style>
