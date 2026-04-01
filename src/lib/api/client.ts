import type { HealthResponse, ModelsResponse, SessionSummary, ChatCompletionRequest } from './types'

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

export async function createSession(model: string): Promise<SessionSummary> {
  const res = await fetch('/v1/sessions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ model }),
  })
  if (!res.ok) {
    throw new Error(`Session creation failed: ${res.status}`)
  }
  return res.json()
}

export async function sendChatMessage(
  request: ChatCompletionRequest
): Promise<Response> {
  const res = await fetch('/v1/chat/completions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(request),
  })
  if (!res.ok) {
    throw new Error(`Chat request failed: ${res.status}`)
  }
  return res
}
