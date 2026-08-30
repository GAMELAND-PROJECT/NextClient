'use strict';

const rows = document.querySelector('#tag-rows');
const template = document.querySelector('#tag-row-template');
const addButton = document.querySelector('#add-row');

function bindRemove(button) {
  button.addEventListener('click', () => button.closest('.tag-row')?.remove());
}

document.querySelectorAll('.remove-row').forEach(bindRemove);
addButton?.addEventListener('click', () => {
  if (!rows || !template || rows.children.length >= 256) return;
  const fragment = template.content.cloneNode(true);
  const button = fragment.querySelector('.remove-row');
  if (button) bindRemove(button);
  rows.appendChild(fragment);
  rows.lastElementChild?.querySelector('input')?.focus();
});

if ('serviceWorker' in navigator && window.isSecureContext) {
  window.addEventListener('load', () => {
    navigator.serviceWorker.register('./service-worker.js', { scope: './' }).catch(() => {});
  });
}

const installButton = document.querySelector('#pwa-install');
let installPrompt = null;
const standalone = window.matchMedia('(display-mode: standalone)').matches ||
  window.navigator.standalone === true;
const ios = /iphone|ipad|ipod/i.test(navigator.userAgent);

if (installButton && ios && !standalone) {
  installButton.hidden = false;
  installButton.addEventListener('click', () => {
    window.alert('در Safari دکمه Share را بزنید و سپس Add to Home Screen را انتخاب کنید.');
  }, { once: true });
}

window.addEventListener('beforeinstallprompt', event => {
  event.preventDefault();
  installPrompt = event;
  if (installButton && !standalone) installButton.hidden = false;
});

installButton?.addEventListener('click', async () => {
  if (!installPrompt) return;
  installButton.disabled = true;
  await installPrompt.prompt();
  await installPrompt.userChoice;
  installPrompt = null;
  installButton.hidden = true;
  installButton.disabled = false;
});

window.addEventListener('appinstalled', () => {
  if (installButton) installButton.hidden = true;
});
