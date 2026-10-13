import { defineConfig } from 'astro/config';
import mdx from '@astrojs/mdx';
import pagefind from 'astro-pagefind';
import rehypeGlossary from './src/plugins/rehype-glossary.mjs';

// For GitHub Pages project sites set SITE and BASE, e.g.
//   SITE=https://<user>.github.io  BASE=/<repo>
export default defineConfig({
  site: process.env.SITE || 'http://localhost:4321',
  base: process.env.BASE || '/',
  integrations: [mdx(), pagefind()],
  markdown: { rehypePlugins: [rehypeGlossary], shikiConfig: { themes: { light: 'github-light', dark: 'github-dark' } } },
});
