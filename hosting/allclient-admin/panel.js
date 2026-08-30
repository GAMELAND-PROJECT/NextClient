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

