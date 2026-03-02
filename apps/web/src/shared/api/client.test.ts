import { afterEach, describe, expect, it, vi } from 'vitest';

import { ApiError, requestJson } from './client';

afterEach(() => {
  vi.restoreAllMocks();
});

describe('requestJson', () => {
  it('returns parsed JSON on success', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn().mockResolvedValue({ ok: true, json: async () => ({ ok: true }) })
    );

    const result = await requestJson<{ ok: boolean }>('/api/test');
    expect(result.ok).toBe(true);
  });

  it('throws ApiError with API message when available', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn().mockResolvedValue({
        ok: false,
        status: 429,
        json: async () => ({ error: { message: 'Too many requests' } })
      })
    );

    await expect(requestJson('/api/test')).rejects.toMatchObject({
      name: 'ApiError',
      status: 429,
      message: 'Too many requests'
    });
  });

  it('falls back to status message when body is not JSON', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn().mockResolvedValue({
        ok: false,
        status: 502,
        json: async () => {
          throw new Error('no json');
        }
      })
    );

    await expect(requestJson('/api/test')).rejects.toMatchObject({
      status: 502,
      message: 'status 502'
    });
  });
});
