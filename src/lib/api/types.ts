export interface HealthResponse {
  status: string
  ready: boolean
  version: string
  model_id: string
}

// Models
export interface ModelObject {
  id: string
  object: string
  owned_by: string
}

export interface ModelsResponse {
  object: string
  data: ModelObject[]
}

// Messages
export interface ChatMessage {
  role: 'system' | 'user' | 'assistant'
  content: string
}

// Chat completion request
export interface ChatCompletionRequest {
  model: string
  messages: ChatMessage[]
  stream?: boolean
  session_id?: string
}

// Streaming chunk
export interface ChatCompletionStreamChunk {
  id: string
  object: string
  created: number
  model: string
  choices: Array<{
    index: number
    delta: {
      role?: string
      content?: string
    }
    finish_reason: string | null
  }>
}

// Sessions
export interface SessionCreateRequest {
  model: string
}

export interface SessionSummary {
  id: string
  object: string
  model: string
  created: number
  last_used: number
  expires_at: number
}
