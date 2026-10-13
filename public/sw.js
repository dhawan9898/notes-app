// DeepNotes service worker: cache-as-you-browse. Pages and assets the reader has opened keep working offline.
const CACHE = 'deepnotes-v1';
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (e) => e.waitUntil(
  caches.keys().then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k)))).then(() => self.clients.claim())
));
self.addEventListener('fetch', (e) => {
  const req = e.request;
  if (req.method !== 'GET' || new URL(req.url).origin !== location.origin) return;
  // pages: network first (fresh content), fall back to the cache when offline; assets: cache first, refresh in the background
  const isPage = req.mode === 'navigate' || (req.headers.get('accept') || '').includes('text/html');
  e.respondWith((async () => {
    const cache = await caches.open(CACHE);
    if (isPage) {
      try { const res = await fetch(req); if (res.ok) cache.put(req, res.clone()); return res; }
      catch { return (await cache.match(req)) || (await cache.match(req, { ignoreSearch: true })) || Response.error(); }
    }
    const hit = await cache.match(req);
    const net = fetch(req).then((res) => { if (res.ok) cache.put(req, res.clone()); return res; }).catch(() => hit);
    return hit || net;
  })());
});
