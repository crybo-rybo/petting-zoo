import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import type { HealthResponse, ModelsResponse } from './types'

// Will import once implemented
// import { fetchHealth } from './client'

describe('fetchHealth', () => {
  const mockHealth: HealthResponse = {
    status: 'ready',
    ready: true,
    version: '0.0.4',
    model_id: 'local-model',
  }

  beforeEach(() => {
    vi.stubGlobal('fetch', vi.fn())
  })

  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('returns parsed health data on 200', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response(JSON.stringify(mockHealth), { status: 200 })
    )

    const { fetchHealth } = await import('./client')
    const result = await fetchHealth()

    expect(fetch).toHaveBeenCalledWith('/healthz')
    expect(result).toEqual(mockHealth)
  })

  it('throws on non-OK response', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response('Service Unavailable', { status: 503 })
    )

    const { fetchHealth } = await import('./client')
    await expect(fetchHealth()).rejects.toThrow('Health check failed: 503')
  })

  it('throws on network error', async () => {
    vi.mocked(fetch).mockRejectedValue(new TypeError('Failed to fetch'))

    const { fetchHealth } = await import('./client')
    await expect(fetchHealth()).rejects.toThrow('Failed to fetch')
  })
})

describe('fetchModels', () => {
  const mockModels: ModelsResponse = {
    object: 'list',
    data: [
      { id: 'local-model', object: 'model', owned_by: 'zoo-keeper-server' },
    ],
  }

  beforeEach(() => {
    vi.stubGlobal('fetch', vi.fn())
  })

  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('returns parsed models data on 200', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response(JSON.stringify(mockModels), { status: 200 })
    )

    const { fetchModels } = await import('./client')
    const result = await fetchModels()

    expect(fetch).toHaveBeenCalledWith('/v1/models')
    expect(result).toEqual(mockModels)
  })

  it('throws on non-OK response', async () => {
    vi.mocked(fetch).mockResolvedValue(
      new Response('Not Found', { status: 404 })
    )

    const { fetchModels } = await import('./client')
    await expect(fetchModels()).rejects.toThrow('Models fetch failed: 404')
  })
})
