'use strict';

const CACHE_NAME = 'allclient-admin-static-v1';
const STATIC_ASSETS = [
  './style.css',
  './panel.js',
  './manifest.webmanifest',
  './app-icon.ico',
  './icon-192.png',
  './icon-512.png'
];

self.addEventListener('install', event => {
  event.waitUntil(caches.open(CACHE_NAME).then(cache => cache.addAll(STATIC_ASSETS)));
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.filter(key => key !== CACHE_NAME).map(key => caches.delete(key))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', event => {
  const request = event.request;
  if (request.method !== 'GET') return;

  const url = new URL(request.url);
  if (url.origin !== self.location.origin) return;

  // Never cache PHP pages, navigations, sessions, form responses or data files.
  if (request.mode === 'navigate' || url.pathname.endsWith('.php') ||
      url.pathname.endsWith('pinned_servers.txt') ||
      url.pathname.endsWith('client_tags.txt') ||
      url.pathname.endsWith('server_password.txt')) {
    event.respondWith(fetch(request, { cache: 'no-store' }));
    return;
  }

  if (STATIC_ASSETS.some(asset => url.pathname.endsWith(asset.slice(1)))) {
    event.respondWith(caches.match(request).then(cached => cached || fetch(request)));
  }
});
