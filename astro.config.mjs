import { defineConfig } from 'astro/config';
import rehypeGlossary from './src/plugins/rehype-glossary.mjs';
import rehypeRunnable from './src/plugins/rehype-runnable.mjs';
import { rehypeShikiCompact } from './src/plugins/shiki-compact.mjs';

// For GitHub Pages project sites set SITE and BASE, e.g.  SITE=https://<user>.github.io  BASE=/<repo>
export default defineConfig({
  site: process.env.SITE || 'http://localhost:4321',
  base: process.env.BASE || '/',
  trailingSlash: 'always',
  build: { inlineStylesheets: 'auto' },
  markdown: { rehypePlugins: [rehypeGlossary, rehypeRunnable, rehypeShikiCompact], shikiConfig: { themes: { light: 'github-light', dark: 'github-dark' } } },
});
