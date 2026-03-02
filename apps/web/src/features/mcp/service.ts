import { requestJson } from '../../shared/api/client';
import type {
  McpConnectionStatus,
  McpConnectorsResponse,
} from '../../shared/api/types';

export async function listMcpConnectors() {
  return requestJson<McpConnectorsResponse>('/api/mcp/connectors');
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
