import { requestJson } from '../../shared/api/client';
import type {
  McpConnectionStatus,
  McpConnector,
  McpConnectorsResponse,
  McpRemoveResponse,
} from '../../shared/api/types';

export async function listMcpConnectors() {
  return requestJson<McpConnectorsResponse>('/api/mcp/connectors');
}

export async function addMcpConnector(input: Pick<McpConnector, 'id' | 'command' | 'args'>) {
  return requestJson<McpConnector>('/api/mcp/connectors', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(input),
  });
}

export async function removeMcpConnector(id: string) {
  return requestJson<McpRemoveResponse>(`/api/mcp/connectors/${encodeURIComponent(id)}`, {
    method: 'DELETE',
  });
}

export async function connectMcpConnector(id: string) {
  return requestJson<McpConnectionStatus>(
    `/api/mcp/connectors/${encodeURIComponent(id)}/connect`,
    { method: 'POST' },
  );
}

export async function disconnectMcpConnector(id: string) {
  return requestJson(`/api/mcp/connectors/${encodeURIComponent(id)}/disconnect`, {
    method: 'POST',
  });
}
