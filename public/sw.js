// DeepNotes service worker: the site keeps working offline for everything the reader has opened.
//  - pages: network first (fresh content), the cached copy when offline
//  - hashed assets (/_astro/…) and the compiler files: cache first, they never change under the same name
//  - everything else (search index, manifest, icon): serve the cached copy at once and refresh it in the background
// Bump VERSION to drop every old cache on the next visit.
const VERSION = 'v2';
const PAGES = `dn-pages-${VERSION}`, ASSETS = `dn-assets-${VERSION}`, MISC = `dn-misc-${VERSION}`;
const MAX_PAGES = 250;

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (e) => e.waitUntil((async () => {
  const keep = new Set([PAGES, ASSETS, MISC]);
  for (const k of await caches.keys()) if (!keep.has(k)) await caches.delete(k);
  await self.clients.claim();
})()));

async function trim(cache, max) { const keys = await cache.keys(); for (let i = 0; i < keys.length - max; i++) await cache.delete(keys[i]); }

self.addEventListener('fetch', (e) => {
  const req = e.request, url = new URL(req.url);
  if (req.method !== 'GET' || url.origin !== location.origin || req.headers.has('range')) return;
  const isPage = req.mode === 'navigate' || (req.headers.get('accept') || '').includes('text/html');
  e.respondWith((async () => {
    if (isPage) {
      const cache = await caches.open(PAGES);
      try { const res = await fetch(req); if (res.ok) { cache.put(req, res.clone()).then(() => trim(cache, MAX_PAGES)); } return res; }
      catch { return (await cache.match(req)) || (await cache.match(req, { ignoreSearch: true })) || Response.error(); }
    }
    if (url.pathname.includes('/_astro/') || url.pathname.includes('/runner/')) {
      const cache = await caches.open(ASSETS), hit = await cache.match(req);
      if (hit) return hit;
      const res = await fetch(req); if (res.ok) cache.put(req, res.clone()); return res;
    }
    const cache = await caches.open(MISC), hit = await cache.match(req);
    const net = fetch(req).then((res) => { if (res.ok) cache.put(req, res.clone()); return res; }).catch(() => hit);
    return hit || net;
  })());
});
