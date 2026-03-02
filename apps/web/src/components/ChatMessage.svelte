<script lang="ts">
  import DOMPurify from 'dompurify';

  import { renderAssistantMarkdown } from '../lib/markdown_render';
  import type { ChatMessage as ChatMessageType } from '../shared/api/types';

  export let message: ChatMessageType;
  export let index = 0;
  export let isStreaming = false;
  export let isLast = false;
  export let onCopy: (event: MouseEvent) => void = () => {};

  function onKeyDown(event: KeyboardEvent) {
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault();
      onCopy(event as unknown as MouseEvent);
    }
  }
</script>

<div class="message {message.role} slide-up" id="message-{index}">
  <div class="message-header">
    <strong>{message.role === 'user' ? 'You' : 'Zoo Model'}</strong>
  </div>
  {#if message.role === 'assistant'}
    <div
      class="markdown-body {isStreaming && isLast ? 'streaming' : ''}"
      role="button"
      tabindex="0"
      on:click={onCopy}
      on:keydown={onKeyDown}
    >
      {@html DOMPurify.sanitize(renderAssistantMarkdown(message.content))}
    </div>
  {:else}
    <pre>{message.content}</pre>
  {/if}
</div>
