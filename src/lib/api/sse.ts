import type { ChatCompletionStreamChunk } from './types'

export async function* parseSSEStream(
  stream: ReadableStream<Uint8Array>
): AsyncGenerator<ChatCompletionStreamChunk> {
  const reader = stream.pipeThrough(new TextDecoderStream()).getReader()
  let buffer = ''

  try {
    while (true) {
      const { done, value } = await reader.read()
      if (done) break

      buffer += value

      const parts = buffer.split('\n\n')
      // Last element may be incomplete — keep it in buffer
      buffer = parts.pop() ?? ''

      for (const part of parts) {
        const line = part.trim()
        if (!line) continue
        if (!line.startsWith('data: ')) continue

        const data = line.slice(6) // strip "data: "
        if (data === '[DONE]') return

        yield JSON.parse(data) as ChatCompletionStreamChunk
      }
    }
  } finally {
    reader.releaseLock()
  }
}
