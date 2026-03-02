import { afterEach, describe, expect, it, vi } from 'vitest';

import { selectModel } from './service';

afterEach(() => {
  vi.restoreAllMocks();
});

describe('models service', () => {
  it('sends context_size when provided', async () => {
    const fetchMock = vi.fn().mockResolvedValue({
      ok: true,
      json: async () => ({ active_model: { id: 'm1' } })
    });
    vi.stubGlobal('fetch', fetchMock);

    await selectModel('m1', 4096);

    expect(fetchMock).toHaveBeenCalledTimes(1);
    const [, init] = fetchMock.mock.calls[0] as [RequestInfo, RequestInit];
    expect(init.method).toBe('POST');
    expect(String(init.body)).toContain('"context_size":4096');
  });

  it('omits context_size when not provided', async () => {
    const fetchMock = vi.fn().mockResolvedValue({
      ok: true,
      json: async () => ({ active_model: { id: 'm1' } })
    });
    vi.stubGlobal('fetch', fetchMock);

    await selectModel('m1');

    const [, init] = fetchMock.mock.calls[0] as [RequestInfo, RequestInit];
    expect(String(init.body)).toBe('{"model_id":"m1"}');
  });
});
