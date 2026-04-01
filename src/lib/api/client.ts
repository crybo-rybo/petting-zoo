import type { HealthResponse, ModelsResponse } from './types'

export async function fetchHealth(): Promise<HealthResponse> {
  const res = await fetch('/healthz')
  if (!res.ok) {
    throw new Error(`Health check failed: ${res.status}`)
  }
  return res.json()
}

export async function fetchModels(): Promise<ModelsResponse> {
  const res = await fetch('/v1/models')
  if (!res.ok) {
    throw new Error(`Models fetch failed: ${res.status}`)
  }
  return res.json()
}
