// SPDX-License-Identifier: LGPL-2.1-or-later
//
// The website's one script. Every page is complete without it; it only adds
// what needs a script: copy buttons, clickable build rows, the Builds
// page's filters and the build page's category filter. Controls that need
// it are hidden in the HTML (hidden, data-js) and shown here. It loads
// nothing and talks to no other site.
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

  /* Builds: filter the list by flavour, channel and Wine version. */
  $$('.filters[data-list]').forEach(bar => {
    const list = document.getElementById(bar.dataset.list);
    if (!list) return;
    bar.hidden = false;
    const f = { f: '', ch: '', v: '' };
    const rows = $$('.rw', list);
    const apply = () => {
      let n = 0;
      rows.forEach(rw => {
        const ok = (!f.f || rw.dataset.f === f.f) && (!f.ch || rw.dataset.ch === f.ch) &&
          (!f.v || rw.dataset.v.startsWith(f.v));
        rw.hidden = !ok;
        n += ok;
      });
      $('[data-count]', bar).textContent = n === rows.length ? `${n} builds` : `${n} of ${rows.length} builds`;
      const empty = $('[data-empty]', list);
      if (empty) empty.hidden = n > 0;
    };
    $$('.seg[data-filter]', bar).forEach(seg => seg.addEventListener('click', e => {
      const b = e.target.closest('button');
      if (!b) return;
      $$('button', seg).forEach(x => x.setAttribute('aria-pressed', String(x === b)));
      f[seg.dataset.filter] = b.dataset.v;
      apply();
    }));
    const input = $('input[data-filter]', bar);
    if (input) input.addEventListener('input', () => { f.v = input.value.trim(); apply(); });
    apply();
  });

  /* Build page: show only the patches of the chosen categories. */
  $$('.pfilters[data-js]').forEach(bar => {
    const root = bar.parentElement;
    bar.hidden = false;
    const on = new Set();
    const rows = $$('.prow[data-cat]', root);
    const apply = () => {
      let shown = 0;
      $$('.pcat', root).forEach(sec => {
        const rs = $$('.prow[data-cat]', sec);
        let n = 0;
        rs.forEach(r => { const ok = !on.size || on.has(r.dataset.cat); r.hidden = !ok; n += ok; });
        const c = $('[data-n]', sec);
        if (c) c.textContent = n === rs.length ? `${rs.length}` : `${n} of ${rs.length}`;
        sec.hidden = n === 0;
        shown += n;
      });
      $('[data-count]', bar).textContent = shown === rows.length ? `${shown} patches` : `${shown} of ${rows.length} patches`;
      const empty = $('[data-empty]', root);
      if (empty) empty.hidden = shown > 0;
    };
    bar.addEventListener('click', e => {
      const b = e.target.closest('button[data-cat]');
      if (!b) return;
      on.has(b.dataset.cat) ? on.delete(b.dataset.cat) : on.add(b.dataset.cat);
      b.setAttribute('aria-pressed', String(on.has(b.dataset.cat)));
      apply();
    });
  });

  wireCopy();
})();
