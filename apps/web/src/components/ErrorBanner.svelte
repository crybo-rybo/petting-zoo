<script lang="ts">
  export let message = '';
  export let prefix = 'Error';
</script>

{#if message}
  <p class="error slide-up">{prefix}: {message}</p>
{/if}
