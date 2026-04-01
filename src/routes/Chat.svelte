<script lang="ts">
  import { onMount } from 'svelte'
  import { fetchModels, createSession, sendChatMessage } from '../lib/api/client'
  import { parseSSEStream } from '../lib/api/sse'
  import type { ChatMessage } from '../lib/api/types'

  interface Props {
    serverReady: boolean
  }

  let { serverReady }: Props = $props()

  // State
  let messages: Array<{ role: 'user' | 'assistant'; content: string }> = $state([])
  let inputText: string = $state('')
  let isStreaming: boolean = $state(false)
  let sessionId: string | null = $state(null)
  let modelId: string | null = $state(null)
  let error: string | null = $state(null)
  let messagesContainer: HTMLDivElement | undefined = $state(undefined)

  const inputDisabled = $derived(!serverReady || isStreaming || !modelId)
  const placeholderText = $derived(
    !serverReady
      ? 'Server unreachable'
      : !modelId
        ? 'No model loaded'
        : isStreaming
          ? 'Waiting for response...'
          : 'Type a message...'
  )

  onMount(async () => {
    try {
      const modelsResponse = await fetchModels()
      if (modelsResponse.data.length > 0) {
        modelId = modelsResponse.data[0].id
      }
    } catch {
      // Model info will show as unavailable
    }
  })

  function scrollToBottom() {
    if (messagesContainer) {
      messagesContainer.scrollTop = messagesContainer.scrollHeight
    }
  }

  async function ensureSession(): Promise<string> {
    if (sessionId) return sessionId
    if (!modelId) throw new Error('No model loaded')
    const session = await createSession(modelId)
    sessionId = session.id
    return session.id
  }

  async function handleSend() {
    const text = inputText.trim()
    if (!text || inputDisabled) return

    inputText = ''
    error = null

    // Add user message immediately
    messages.push({ role: 'user', content: text })
    // Add empty assistant message for streaming
    const assistantIndex = messages.length
    messages.push({ role: 'assistant', content: '' })

    // Scroll after adding messages
    await tick()
    scrollToBottom()

    isStreaming = true

    try {
      let currentSessionId: string
      try {
        currentSessionId = await ensureSession()
      } catch (e) {
        throw new Error(
          e instanceof Error ? e.message : 'Failed to create session'
        )
      }

      const chatMessage: ChatMessage = { role: 'user', content: text }

      let response: Response
      try {
        response = await sendChatMessage({
          model: modelId!,
          messages: [chatMessage],
          stream: true,
          session_id: currentSessionId,
        })
      } catch (e) {
        // Handle 503 — server busy
        if (e instanceof Error && e.message.includes('503')) {
          throw new Error('Server busy, try again')
        }
        // Handle session expired — auto-recreate
        if (e instanceof Error && e.message.includes('404')) {
          sessionId = null
          const newSessionId = await ensureSession()

          // Add a notice about session expiry
          messages[assistantIndex].content =
            'Session expired — starting a new conversation.\n\n'

          response = await sendChatMessage({
            model: modelId!,
            messages: [chatMessage],
            stream: true,
            session_id: newSessionId,
          })
        } else {
          throw e
        }
      }

      if (!response.body) {
        throw new Error('No response stream')
      }

      for await (const chunk of parseSSEStream(response.body)) {
        const delta = chunk.choices[0]?.delta
        if (delta?.content) {
          messages[assistantIndex].content += delta.content
          scrollToBottom()
        }
      }
    } catch (e) {
      const errorMsg = e instanceof Error ? e.message : 'Unknown error'
      if (messages[assistantIndex].content === '') {
        messages[assistantIndex].content = `Error: ${errorMsg}`
      } else {
        messages[assistantIndex].content += `\n\nError: ${errorMsg}`
      }
      error = errorMsg
    } finally {
      isStreaming = false
      scrollToBottom()
    }
  }

  function handleKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault()
      handleSend()
    }
  }
</script>

<script module lang="ts">
  import { tick } from 'svelte'
</script>

<div class="chat">
  <div class="chat-header">
    <span class="model-label">
      {modelId ?? 'No model loaded'}
    </span>
  </div>

  <div class="chat-messages" bind:this={messagesContainer}>
    {#if messages.length === 0}
      <div class="empty-state">
        <p>Send a message to start chatting.</p>
      </div>
    {/if}

    {#each messages as msg}
      <div class="message" class:assistant={msg.role === 'assistant'}>
        <div class="message-role">
          {msg.role === 'user' ? 'You' : 'Assistant'}
        </div>
        <div class="message-content">{msg.content}</div>
      </div>
    {/each}
  </div>

  <div class="chat-input">
    <textarea
      bind:value={inputText}
      onkeydown={handleKeydown}
      placeholder={placeholderText}
      disabled={inputDisabled}
      rows="1"
    ></textarea>
    <button
      class="send-btn"
      onclick={handleSend}
      disabled={inputDisabled || !inputText.trim()}
      title="Send message"
    >
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <line x1="22" y1="2" x2="11" y2="13"/>
        <polygon points="22 2 15 22 11 13 2 9 22 2"/>
      </svg>
    </button>
  </div>
</div>

<style>
  .chat {
    display: flex;
    flex-direction: column;
    height: 100vh;
    font-family: system-ui, -apple-system, sans-serif;
  }

  .chat-header {
    padding: 12px 20px;
    border-bottom: 1px solid #333;
    display: flex;
    justify-content: flex-end;
    flex-shrink: 0;
  }

  .model-label {
    color: #666;
    font-size: 0.8rem;
    font-family: ui-monospace, monospace;
  }

  .chat-messages {
    flex: 1;
    overflow-y: auto;
    min-height: 0;
  }

  .empty-state {
    display: flex;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: #666;
  }

  .message {
    padding: 16px 20px;
    border-bottom: 1px solid #222;
  }

  .message.assistant {
    background: #1a1a1a;
  }

  .message-role {
    color: #999;
    font-size: 0.7rem;
    font-weight: 600;
    text-transform: uppercase;
    margin-bottom: 6px;
  }

  .message-content {
    color: #e5e5e5;
    line-height: 1.6;
    white-space: pre-wrap;
    word-break: break-word;
  }

  .chat-input {
    padding: 12px 16px;
    border-top: 1px solid #333;
    display: flex;
    gap: 8px;
    align-items: flex-end;
    flex-shrink: 0;
  }

  textarea {
    flex: 1;
    background: #222;
    border: 1px solid #444;
    border-radius: 8px;
    padding: 10px 12px;
    color: #e5e5e5;
    font-family: system-ui, -apple-system, sans-serif;
    font-size: 0.9rem;
    resize: none;
    line-height: 1.5;
    min-height: 40px;
    max-height: 120px;
  }

  textarea::placeholder {
    color: #666;
  }

  textarea:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  textarea:focus {
    outline: none;
    border-color: #666;
  }

  .send-btn {
    width: 40px;
    height: 40px;
    border: none;
    border-radius: 8px;
    background: #2563eb;
    color: white;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    transition: background 0.15s;
  }

  .send-btn:hover:not(:disabled) {
    background: #1d4ed8;
  }

  .send-btn:disabled {
    background: #333;
    color: #666;
    cursor: not-allowed;
  }
</style>
