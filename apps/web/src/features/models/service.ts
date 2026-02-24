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

export async function selectModel(modelId: string) {
  return requestJson<SelectModelResponse>('/api/models/select', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ model_id: modelId }),
  });
}

export async function unloadModel() {
  return requestJson<UnloadModelResponse>('/api/models/unload', { method: 'POST' });
}
