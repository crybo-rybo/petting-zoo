export type ModelSummary = {
  id: string;
  display_name?: string;
  path?: string;
  status?: string;
  context_size?: number;
};

export type ListModelsResponse = {
  active_model_id: string | null;
};

export type RegisterModelResponse = {
  model: Pick<ModelSummary, 'id'>;
};

export type SelectModelResponse = {
  active_model: Pick<ModelSummary, 'id'>;
};

export type UnloadModelResponse = {
  status: string;
};

export type ChatUsage = {
  prompt_tokens: number;
  completion_tokens: number;
  total_tokens: number;
};

export type ChatMetrics = {
  latency_ms: number;
  time_to_first_token_ms: number;
  tokens_per_second: number;
};

export type ChatResetResponse = {
  status: string;
  model_id: string;
};

export type ClearMemoryResponse = {
  status: string;
  model_id: string;
};

export type McpConnector = {
  id: string;
  command: string;
  args: string[];
};

export type McpConnectorsResponse = {
  connectors: McpConnector[];
};

export type McpConnectionStatus = {
  server_id: string;
  connected: boolean;
  discovered_tool_count: number;
};

export type McpRemoveResponse = {
  status: string;
  id: string;
};
