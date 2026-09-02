'use strict';

document.documentElement.classList.add('tabs-ready');
const panelTabs = [...document.querySelectorAll('.panel-tab')];
const panelViews = [...document.querySelectorAll('.panel-view')];
function activatePanel(name, remember = true) {
  if (!panelViews.some(view => view.dataset.panelView === name)) name = 'dashboard';
  panelTabs.forEach(tab => {
    const selected = tab.dataset.panel === name;
    tab.classList.toggle('active', selected);
    tab.setAttribute('aria-selected', selected ? 'true' : 'false');
  });
  panelViews.forEach(view => view.classList.toggle('active', view.dataset.panelView === name));
  if (remember) {
    try { sessionStorage.setItem('allclient-active-panel', name); } catch (_) {}
    history.replaceState(null, '', `#${name}`);
  }
}
panelTabs.forEach(tab => tab.addEventListener('click', () => activatePanel(tab.dataset.panel || 'dashboard')));
let initialPanel = window.location.hash.slice(1);
if (!initialPanel) {
  try { initialPanel = sessionStorage.getItem('allclient-active-panel') || ''; } catch (_) {}
}
activatePanel(initialPanel || 'dashboard', false);

const addPanel = document.querySelector('#add-subscription');
const showAdd = document.querySelector('#show-add-subscription');
const closeAdd = document.querySelector('#close-add-subscription');
showAdd?.addEventListener('click', () => {
  if (!addPanel) return;
  activatePanel('subscriptions');
  addPanel.hidden = false;
  addPanel.scrollIntoView({ behavior: 'smooth', block: 'center' });
  addPanel.querySelector('input[name="build_tag"]')?.focus();
});
closeAdd?.addEventListener('click', () => { if (addPanel) addPanel.hidden = true; });

document.querySelectorAll('.confirm-form').forEach(form => {
  form.addEventListener('submit', event => {
    const message = form.dataset.confirm || 'از انجام این عملیات مطمئن هستید؟';
    if (!window.confirm(message)) event.preventDefault();
  });
});

const cards = [...document.querySelectorAll('.subscription-card')];
const search = document.querySelector('#subscription-search');
const filters = [...document.querySelectorAll('.filter-chip')];
let activeFilter = 'all';
const modalTriggers = [...document.querySelectorAll('.open-profile-modal')];
function closeProfileModal(dialog) {
  if (!dialog?.open) return;
  if (typeof dialog.close === 'function') dialog.close();
  else dialog.removeAttribute('open');
}
modalTriggers.forEach(trigger => trigger.addEventListener('click', () => {
  const dialog = document.getElementById(trigger.dataset.modal || '');
  if (!dialog) return;
  if (typeof dialog.showModal === 'function') dialog.showModal();
  else dialog.setAttribute('open', '');
}));
document.querySelectorAll('.profile-modal').forEach(dialog => {
  dialog.querySelector('[data-close-modal]')?.addEventListener('click', () => closeProfileModal(dialog));
  dialog.addEventListener('click', event => {
    if (event.target === dialog) closeProfileModal(dialog);
  });
});
function applySubscriptionFilter() {
  const query = (search?.value || '').trim().toLocaleLowerCase('fa');
  cards.forEach(card => {
    const stateMatches = activeFilter === 'all' || card.dataset.state === activeFilter;
    const textMatches = !query || (card.dataset.search || '').toLocaleLowerCase('fa').includes(query);
    card.hidden = !(stateMatches && textMatches);
  });
}
search?.addEventListener('input', applySubscriptionFilter);
filters.forEach(button => button.addEventListener('click', () => {
  activeFilter = button.dataset.filter || 'all';
  filters.forEach(item => item.classList.toggle('active', item === button));
  applySubscriptionFilter();
}));

if ('serviceWorker' in navigator && window.isSecureContext) {
  window.addEventListener('load', () => {
    navigator.serviceWorker.register('./service-worker.js', { scope: './' }).catch(() => {});
  });
}
const installButton = document.querySelector('#pwa-install');
let installPrompt = null;
const standalone = window.matchMedia('(display-mode: standalone)').matches || window.navigator.standalone === true;
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
window.addEventListener('appinstalled', () => { if (installButton) installButton.hidden = true; });
