import hljs from 'highlight.js';
import { marked } from 'marked';

const escapeHtml = (value: string): string =>
  value.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;');

const normalizeLanguage = (lang: string | null | undefined): string => {
  const candidate = (lang ?? '').trim().toLowerCase();
  if (!candidate) return 'plaintext';
  return hljs.getLanguage(candidate) ? candidate : 'plaintext';
};

const renderer = {
  code(token: any) {
    const language = normalizeLanguage(token.lang);
    const highlighted = hljs.highlight(token.text ?? '', { language }).value;
    return [
      '<div class="code-block-wrapper">',
      '<div class="code-header">',
      `<span class="lang-label">${escapeHtml(language)}</span>`,
      '<button class="copy-btn">Copy</button>',
      '</div>',
      `<pre><code class="hljs language-${escapeHtml(language)}">${highlighted}</code></pre>`,
      '</div>'
    ].join('');
  },
  html(token: any) {
    // Raw HTML-like text should render literally in chat output.
    return escapeHtml(token.raw ?? token.text ?? '');
  }
};

marked.use({ renderer });

export const renderAssistantMarkdown = (content: string): string =>
  (marked.parse(content ?? '') as string);
