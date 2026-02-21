export type Usage = {
  prompt_tokens: number;
  completion_tokens: number;
  total_tokens: number;
};

export type Metrics = {
  latency_ms: number;
  time_to_first_token_ms: number;
  tokens_per_second: number;
};

export function usageSummary(usage: Usage): string {
  return `prompt=${usage.prompt_tokens}, completion=${usage.completion_tokens}, total=${usage.total_tokens}`;
}

export function metricsSummary(metrics: Metrics): string {
  return `latency=${metrics.latency_ms}ms, ttfb=${metrics.time_to_first_token_ms}ms, tps=${metrics.tokens_per_second.toFixed(2)}`;
}
