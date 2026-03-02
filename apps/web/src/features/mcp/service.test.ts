import { afterEach, describe, expect, it, vi } from 'vitest';

import { connectMcpConnector, disconnectMcpConnector } from './service';

afterEach(() => {
  vi.restoreAllMocks();
});

describe('mcp service', () => {
  it('URL-encodes connector id when connecting', async () => {
    const fetchMock = vi.fn().mockResolvedValue({
      ok: true,
      json: async () => ({ server_id: 'server a', connected: true, discovered_tool_count: 1 })
    });
    vi.stubGlobal('fetch', fetchMock);

    await connectMcpConnector('server a');

    const [input] = fetchMock.mock.calls[0] as [RequestInfo, RequestInit];
    expect(String(input)).toContain('/api/mcp/connectors/server%20a/connect');
  });

  it('URL-encodes connector id when disconnecting', async () => {
    const fetchMock = vi.fn().mockResolvedValue({ ok: true, json: async () => ({ status: 'disconnected' }) });
    vi.stubGlobal('fetch', fetchMock);

    await disconnectMcpConnector('server a');

    const [input] = fetchMock.mock.calls[0] as [RequestInfo, RequestInit];
    expect(String(input)).toContain('/api/mcp/connectors/server%20a/disconnect');
  });
});
