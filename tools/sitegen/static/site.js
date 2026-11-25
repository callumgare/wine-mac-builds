// SPDX-License-Identifier: LGPL-2.1-or-later
//
// The website's one script. Every page is complete without it; it only adds
// what needs a script: copy buttons and clickable build rows, for now.
// Later pages add their filters, the Flavours version switcher and the set
// popovers here. It loads nothing and talks to no other site.
(() => {
  'use strict';
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => [...r.querySelectorAll(s)];

  /* Copy buttons: <button class="copy" data-copy="#id" hidden> copies the
     text of #id. They are hidden in the HTML, since they need this script. */
  function wireCopy(root = document) {
    $$('.copy[data-copy]', root).forEach(b => {
      if (b.dataset.wired) return;
      b.dataset.wired = '1';
      b.hidden = false;
      const label = $('.cl', b);
      b.addEventListener('click', async () => {
        const src = $(b.dataset.copy);
        if (!src) return;
        try {
          await navigator.clipboard.writeText(src.textContent.trim());
        } catch {
          // No clipboard access: select the text so it can be copied by hand.
          const r = document.createRange();
          r.selectNodeContents(src);
          const s = getSelection();
          s.removeAllRanges();
          s.addRange(r);
          return;
        }
        b.classList.add('done');
        if (label) label.textContent = 'Copied';
        clearTimeout(b._t);
        b._t = setTimeout(() => {
          b.classList.remove('done');
          if (label) label.textContent = 'Copy';
        }, 1400);
      });
    });
  }

  /* Build rows: a click anywhere on a row except its links and buttons
     follows the row's build link (keyboard users use the link itself). */
  document.addEventListener('click', e => {
    const row = e.target.closest('.brow');
    if (!row || e.target.closest('a, button, input, label, summary')) return;
    if (getSelection().toString()) return;
    const a = $('.bn a', row);
    if (a) location.href = a.href;
  });

  wireCopy();
})();
