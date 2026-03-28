import type { HealthResponse } from './types'

export async function fetchHealth(): Promise<HealthResponse> {
  const res = await fetch('/healthz')
  if (!res.ok) {
    throw new Error(`Health check failed: ${res.status}`)
  }
  return res.json()
}
