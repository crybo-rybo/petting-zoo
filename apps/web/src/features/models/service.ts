import { requestJson } from '../../shared/api/client';
import type {
  ListModelsResponse,
  RegisterModelResponse,
  SelectModelResponse,
  UnloadModelResponse,
} from '../../shared/api/types';

export async function listModels() {
  return requestJson<ListModelsResponse>('/api/models');
}

export async function registerModel(path: string) {
  return requestJson<RegisterModelResponse>('/api/models/register', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path: path.trim(), display_name: '' }),
  });
}

export async function selectModel(modelId: string, contextSize?: number) {
  const payload: Record<string, unknown> = { model_id: modelId };
  if (contextSize !== undefined) {
    payload.context_size = contextSize;
  }
  return requestJson<SelectModelResponse>('/api/models/select', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
}

export async function unloadModel() {
  return requestJson<UnloadModelResponse>('/api/models/unload', { method: 'POST' });
}
