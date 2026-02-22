const { marked } = require('marked');
const renderer = {
  code(token) {
    console.log("Token properties:", Object.keys(token));
    return `<pre><code>${token.text}</code></pre>`;
  }
};
marked.use({ renderer });
console.log(marked.parse('```python\nprint("hello")\n```'));
