import { describe, expect, it } from 'vitest';

import { renderAssistantMarkdown } from './markdown_render';

describe('renderAssistantMarkdown', () => {
  it('renders fenced code blocks with wrapper and language label', () => {
    const html = renderAssistantMarkdown('```python\nprint("hello")\n```');
    expect(html).toContain('code-block-wrapper');
    expect(html).toContain('lang-label');
    expect(html).toContain('language-python');
    expect(html).toContain('print');
  });

  it('falls back to plaintext for unknown code languages', () => {
    const html = renderAssistantMarkdown('```not-a-real-lang\nx\n```');
    expect(html).toContain('language-plaintext');
  });

  it('escapes raw html-like tokens so special text remains visible', () => {
    const html = renderAssistantMarkdown('Use token <special_tag> literally.');
    expect(html).toContain('&lt;special_tag&gt;');
  });
});
