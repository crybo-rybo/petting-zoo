import { requestJson } from '../../shared/api/client';
import type { ChatMetrics, ChatUsage, ChatResetResponse, ClearMemoryResponse } from '../../shared/api/types';

export type ChatStreamEvent =
  | { type: 'token'; content: string }
  | { type: 'done'; text?: string; usage?: ChatUsage; metrics?: ChatMetrics }
  | { type: 'error'; code?: string; message?: string };

export async function openChatStream(message: string, signal: AbortSignal) {
  const resp = await fetch('/api/chat/stream', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ message }),
    signal,
  });
  if (!resp.ok) {
    let errorMessage = `status ${resp.status}`;
    try {
      const body = await resp.json();
      if (body?.error?.message) errorMessage = body.error.message;
    } catch {
      // Keep status fallback.
    }
    throw new Error(errorMessage);
  }
  if (!resp.body) throw new Error('No response body');
  return resp.body;
}

export async function consumeSseStream(
  body: ReadableStream<Uint8Array>,
  onEvent: (event: ChatStreamEvent) => Promise<void> | void,
) {
  const reader = body.getReader();
  const decoder = new TextDecoder();
  let buffer = '';

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;

    buffer += decoder.decode(value, { stream: true });
    const parts = buffer.split('\n\n');
    buffer = parts.pop() ?? '';

    for (const part of parts) {
      if (!part.startsWith('data: ')) continue;
      const event = JSON.parse(part.slice(6)) as ChatStreamEvent;
      await onEvent(event);
    }
  }
}

export async function resetChat() {
  return requestJson<ChatResetResponse>('/api/chat/reset', { method: 'POST' });
}

export async function clearMemory() {
  return requestJson<ClearMemoryResponse>('/api/chat/clear_memory', { method: 'POST' });
}
