<script lang="ts">
  import { createEventDispatcher, tick } from 'svelte';

  export let value = '';
  export let busy = false;
  export let hasActiveModel = false;
  export let chatStreaming = false;
  export let maxChars = 16000;
  export let canReset = false;

  const dispatch = createEventDispatcher<{ send: void; stop: void; reset: void; wipe: void }>();

  function autoResize(node: HTMLTextAreaElement) {
    function resize() {
      node.style.height = 'auto';
      node.style.height = node.scrollHeight + 'px';
    }
    node.addEventListener('input', resize);
    void tick().then(resize);
    return {
      destroy() {
        node.removeEventListener('input', resize);
      }
    };
  }

  function onKeyDown(event: KeyboardEvent) {
    if (event.key === 'Enter' && !event.shiftKey) {
      event.preventDefault();
      dispatch('send');
    }
  }
</script>

<div class="chat-input-wrapper">
  <div class="chat-input-row">
    <textarea
      bind:value
      rows="1"
      use:autoResize
      style="max-height: 200px; overflow-y: auto;"
      placeholder="Type your message..."
      disabled={busy || !hasActiveModel}
      maxlength={maxChars}
      aria-label="Chat input"
      on:keydown={onKeyDown}
    ></textarea>
    <div class="chat-actions">
      {#if chatStreaming}
        <button class="danger ghost" type="button" on:click={() => dispatch('stop')}>Stop</button>
      {:else}
        <button
          class="primary glow"
          type="button"
          on:click={() => dispatch('send')}
          disabled={busy || !value.trim() || !hasActiveModel}
        >
          Send
        </button>
      {/if}
    </div>
  </div>
  <div class="secondary-actions">
    <button class="ghost action-btn" type="button" on:click={() => dispatch('reset')} disabled={busy || !hasActiveModel || !canReset}>
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8"/><path d="M3 3v5h5"/></svg>
      Reset Session
    </button>
    <button class="ghost danger-text action-btn" type="button" on:click={() => dispatch('wipe')} disabled={busy || !hasActiveModel}>
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18"/><path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"/><path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"/></svg>
      Wipe Memory
    </button>
  </div>
</div>
