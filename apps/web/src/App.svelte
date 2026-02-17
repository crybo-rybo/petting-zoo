<script lang="ts">
  type Health = {
    status: string;
    service: string;
    version: string;
    timestamp: string;
  };

  let health: Health | null = null;
  let error = '';

  async function loadHealth() {
    error = '';
    try {
      const res = await fetch('/healthz');
      if (!res.ok) {
        throw new Error(`status ${res.status}`);
      }
      health = (await res.json()) as Health;
    } catch (e) {
      error = e instanceof Error ? e.message : 'unknown error';
      health = null;
    }
  }

  loadHealth();
</script>

<main class="shell">
  <section class="hero">
    <p class="eyebrow">PHASE 1 SKELETON</p>
    <h1>Petting Zoo</h1>
    <p class="lead">Backend and frontend are wired together. Next step is implementing API slices.</p>
    <button on:click={loadHealth}>Reload Health</button>
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
    {:else if error}
      <p class="error">Failed to load health: {error}</p>
    {:else}
      <p>Loading...</p>
    {/if}
  </section>
</main>

<style>
  :global(:root) {
    --bg-deep: #0f172a;
    --bg-mid: #1e293b;
    --accent: #f97316;
    --accent-soft: #fdba74;
    --paper: #f8fafc;
    --ink: #0f172a;
    --muted: #64748b;
    font-family: 'Space Grotesk', sans-serif;
  }

  :global(body) {
    margin: 0;
    min-height: 100vh;
    color: var(--paper);
    background:
      radial-gradient(1100px circle at 0% 0%, #1d4ed8 0%, transparent 50%),
      radial-gradient(900px circle at 100% 0%, #ea580c 0%, transparent 40%),
      linear-gradient(135deg, var(--bg-deep), var(--bg-mid));
  }

  .shell {
    max-width: 960px;
    margin: 0 auto;
    padding: 3rem 1.25rem;
    display: grid;
    gap: 1rem;
    animation: fade-in 500ms ease-out;
  }

  .hero {
    background: rgb(15 23 42 / 70%);
    border: 1px solid rgb(251 146 60 / 25%);
    border-radius: 20px;
    padding: 2rem;
    backdrop-filter: blur(6px);
  }

  .eyebrow {
    margin: 0;
    letter-spacing: 0.2em;
    font-size: 0.72rem;
    color: var(--accent-soft);
    font-family: 'IBM Plex Mono', monospace;
  }

  h1 {
    margin: 0.35rem 0 0;
    font-size: clamp(2rem, 5vw, 3.4rem);
  }

  .lead {
    margin: 0.75rem 0 0;
    color: #e2e8f0;
    max-width: 60ch;
  }

  button {
    margin-top: 1rem;
    border: 0;
    border-radius: 999px;
    padding: 0.6rem 1rem;
    font-weight: 600;
    background: linear-gradient(90deg, var(--accent), #f59e0b);
    color: #111827;
    cursor: pointer;
  }

  .card {
    background: rgb(248 250 252 / 95%);
    color: var(--ink);
    border-radius: 20px;
    padding: 1.3rem 1.5rem;
  }

  h2 {
    margin-top: 0;
  }

  dl {
    margin: 0;
    display: grid;
    gap: 0.75rem;
  }

  dl div {
    display: grid;
    grid-template-columns: 9rem 1fr;
    gap: 0.5rem;
    border-bottom: 1px dashed #cbd5e1;
    padding-bottom: 0.4rem;
  }

  dt {
    color: var(--muted);
    font-family: 'IBM Plex Mono', monospace;
  }

  .error {
    color: #b91c1c;
  }

  @keyframes fade-in {
    from {
      opacity: 0;
      transform: translateY(14px);
    }
    to {
      opacity: 1;
      transform: translateY(0);
    }
  }

  @media (max-width: 600px) {
    .hero,
    .card {
      border-radius: 14px;
      padding: 1rem;
    }

    dl div {
      grid-template-columns: 1fr;
    }
  }
</style>
