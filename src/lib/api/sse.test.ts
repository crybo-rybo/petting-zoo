import { describe, it, expect } from 'vitest'
import type { ChatCompletionStreamChunk } from './types'

// Helper: create a ReadableStream from a string
function stringToStream(text: string): ReadableStream<Uint8Array> {
  const encoder = new TextEncoder()
  return new ReadableStream({
    start(controller) {
      controller.enqueue(encoder.encode(text))
      controller.close()
    },
  })
}

// Helper: create a ReadableStream that emits chunks separately
function chunksToStream(chunks: string[]): ReadableStream<Uint8Array> {
  const encoder = new TextEncoder()
  return new ReadableStream({
    start(controller) {
      for (const chunk of chunks) {
        controller.enqueue(encoder.encode(chunk))
      }
      controller.close()
    },
  })
}

describe('parseSSEStream', () => {
  const makeChunk = (
    content: string,
    finish: string | null = null,
    role?: string
  ): string => {
    const delta: Record<string, string> = {}
    if (role) delta.role = role
    if (content) delta.content = content
    const chunk: ChatCompletionStreamChunk = {
      id: 'chatcmpl-123',
      object: 'chat.completion.chunk',
      created: 1711900000,
      model: 'local-model',
      choices: [{ index: 0, delta, finish_reason: finish }],
    }
    return `data: ${JSON.stringify(chunk)}\n\n`
  }

  it('parses a single SSE chunk', async () => {
    const sseData = makeChunk('Hello', null, 'assistant') + 'data: [DONE]\n\n'
    const stream = stringToStream(sseData)

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(1)
    expect(chunks[0].choices[0].delta.content).toBe('Hello')
    expect(chunks[0].choices[0].delta.role).toBe('assistant')
  })

  it('parses multiple SSE chunks', async () => {
    const sseData =
      makeChunk('Hello', null, 'assistant') +
      makeChunk(' world') +
      makeChunk('', 'stop') +
      'data: [DONE]\n\n'
    const stream = stringToStream(sseData)

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(3)
    expect(chunks[0].choices[0].delta.content).toBe('Hello')
    expect(chunks[1].choices[0].delta.content).toBe(' world')
    expect(chunks[2].choices[0].finish_reason).toBe('stop')
  })

  it('handles chunks split across read boundaries', async () => {
    const full = makeChunk('Hi', null, 'assistant') + 'data: [DONE]\n\n'
    // Split in the middle of the JSON
    const splitPoint = Math.floor(full.length / 2)
    const stream = chunksToStream([
      full.slice(0, splitPoint),
      full.slice(splitPoint),
    ])

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(1)
    expect(chunks[0].choices[0].delta.content).toBe('Hi')
  })

  it('terminates on [DONE]', async () => {
    const sseData = makeChunk('token1') + 'data: [DONE]\n\n' + makeChunk('token2')
    const stream = stringToStream(sseData)

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(1)
    expect(chunks[0].choices[0].delta.content).toBe('token1')
  })

  it('skips empty lines', async () => {
    const sseData = '\n\n' + makeChunk('Hello', null, 'assistant') + '\n\n' + 'data: [DONE]\n\n'
    const stream = stringToStream(sseData)

    const { parseSSEStream } = await import('./sse')
    const chunks: ChatCompletionStreamChunk[] = []
    for await (const chunk of parseSSEStream(stream)) {
      chunks.push(chunk)
    }

    expect(chunks).toHaveLength(1)
    expect(chunks[0].choices[0].delta.content).toBe('Hello')
  })
})
