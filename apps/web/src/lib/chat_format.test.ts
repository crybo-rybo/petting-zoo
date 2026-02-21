import { describe, expect, it } from 'vitest';

import { metricsSummary, usageSummary } from './chat_format';

describe('chat formatting', () => {
  it('formats usage summary', () => {
    expect(usageSummary({ prompt_tokens: 10, completion_tokens: 20, total_tokens: 30 })).toBe(
      'prompt=10, completion=20, total=30'
    );
  });

  it('formats metrics summary with fixed precision', () => {
    expect(metricsSummary({ latency_ms: 1200, time_to_first_token_ms: 180, tokens_per_second: 9.876 })).toBe(
      'latency=1200ms, ttfb=180ms, tps=9.88'
    );
  });
});
