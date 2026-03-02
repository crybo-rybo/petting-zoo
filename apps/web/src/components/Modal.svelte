<script lang="ts">
  import { createEventDispatcher, tick } from 'svelte';

  export let open = false;
  export let title = '';
  export let closeLabel = 'Close dialog';

  const dispatch = createEventDispatcher<{ close: void }>();

  let dialogEl: HTMLDivElement | null = null;
  let previouslyFocused: HTMLElement | null = null;

  function close() {
    dispatch('close');
  }

  function trapFocus(event: KeyboardEvent) {
    if (!dialogEl) return;
    if (event.key === 'Escape') {
      event.preventDefault();
      close();
      return;
    }
    if (event.key !== 'Tab') return;

    const focusable = dialogEl.querySelectorAll<HTMLElement>(
      'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
    );
    if (focusable.length === 0) {
      event.preventDefault();
      dialogEl.focus();
      return;
    }

    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    const active = document.activeElement as HTMLElement | null;

    if (event.shiftKey && active === first) {
      event.preventDefault();
      last.focus();
      return;
    }

    if (!event.shiftKey && active === last) {
      event.preventDefault();
      first.focus();
    }
  }

  $: if (open) {
    previouslyFocused = document.activeElement as HTMLElement | null;
    void tick().then(() => {
      dialogEl?.focus();
    });
  }

  $: if (!open && previouslyFocused) {
    previouslyFocused.focus();
    previouslyFocused = null;
  }
</script>

{#if open}
  <div class="modal-backdrop fade-in" role="presentation" on:click={close}>
    <div
      class="modal-content slide-up"
      role="dialog"
      aria-modal="true"
      aria-label={title}
      tabindex="-1"
      bind:this={dialogEl}
      on:click|stopPropagation
      on:keydown={trapFocus}
    >
      <div class="modal-header">
        <h3>{title}</h3>
        <button class="ghost action-btn" type="button" aria-label={closeLabel} on:click={close}>✕</button>
      </div>
      <slot />
    </div>
  </div>
{/if}
