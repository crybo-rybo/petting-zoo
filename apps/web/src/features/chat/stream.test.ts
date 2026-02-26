import { describe, expect, it } from 'vitest';

import { consumeSseStream } from './stream';

describe('consumeSseStream', () => {
  it('parses split SSE payloads across chunks', async () => {
    const encoder = new TextEncoder();
    const chunks = [
      encoder.encode('data: {"type":"token","content":"hel'),
      encoder.encode('lo"}\n\ndata: {"type":"done","usage":{"prompt_tokens":1,"completion_tokens":2,"total_tokens":3},"metrics":{"latency_ms":10,"time_to_first_token_ms":4,"tokens_per_second":2.5}}\n\n'),
    ];

    const stream = new ReadableStream<Uint8Array>({
      start(controller) {
        for (const chunk of chunks) controller.enqueue(chunk);
        controller.close();
      },
    });

    const events: Array<{ type: string }> = [];
    await consumeSseStream(stream, (event) => {
      events.push(event as { type: string });
    });

    expect(events).toHaveLength(2);
    expect(events[0].type).toBe('token');
    expect(events[1].type).toBe('done');
  });

  it('ignores malformed JSON gracefully', async () => {
    const encoder = new TextEncoder();
    const chunks = [
      encoder.encode('data: {"type":"token","content":"hel"}\n\ndata: {broken json}\n\ndata: {"type":"token","content":"lo"}\n\n'),
    ];

    const stream = new ReadableStream<Uint8Array>({
      start(controller) {
        for (const chunk of chunks) controller.enqueue(chunk);
        controller.close();
      },
    });

    const events: Array<{ type: string; content?: string }> = [];
    await consumeSseStream(stream, (event) => {
      events.push(event as { type: string; content?: string });
    });

    expect(events).toHaveLength(2);
    expect(events[0].type).toBe('token');
    expect(events[0].content).toBe('hel');
    expect(events[1].type).toBe('token');
    expect(events[1].content).toBe('lo');
  });
});
