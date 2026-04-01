<script lang="ts">
  import Sidebar from './components/Sidebar.svelte'
  import Health from './routes/Health.svelte'
  import Chat from './routes/Chat.svelte'

  let activeView: 'health' | 'chat' = $state('chat')
  let serverReady: boolean = $state(false)

  function handleNavigate(view: 'health' | 'chat') {
    activeView = view
  }

  function handleHealthStatus(ready: boolean) {
    serverReady = ready
  }
</script>

<div class="app-layout">
  <Sidebar
    {activeView}
    onNavigate={handleNavigate}
    onHealthStatus={handleHealthStatus}
  />
  <div class="main-content">
    {#if activeView === 'health'}
      <Health />
    {:else if activeView === 'chat'}
      <Chat {serverReady} />
    {/if}
  </div>
</div>
