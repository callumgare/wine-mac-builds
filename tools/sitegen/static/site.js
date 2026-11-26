// SPDX-License-Identifier: LGPL-2.1-or-later
//
// The website's one script. Every page is complete without it; it only adds
// what needs a script: copy buttons, clickable build rows, the Builds
// page's filters, the patch filters, and the Flavours page's version
// switcher and set popovers. Controls that need
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

  /* Patch lists (a build page, /patches/): filter by the chips in each
     [data-box] (set, cat, origin: a row matches a box if it has any chosen
     value) and by the search box. */
  $$('.pfilters[data-js]').forEach(bar => {
    const root = bar.parentElement;
    bar.hidden = false;
    const chosen = {};
    let q = '';
    const rows = $$('.prow', root);
    const attr = { set: 'set', cat: 'cat', origin: 'origins' };
    const match = r => Object.entries(chosen).every(([box, vals]) => {
      if (!vals.size) return true;
      const have = (r.dataset[attr[box]] || '').split(' ');
      return have.some(v => vals.has(v));
    }) && (!q || (r.dataset.search || r.textContent.toLowerCase()).includes(q));
    const apply = () => {
      let shown = 0;
      $$('.pcat', root).forEach(sec => {
        const rs = $$('.prow', sec);
        let n = 0;
        rs.forEach(r => { const ok = match(r); r.hidden = !ok; n += ok; });
        const c = $('[data-n]', sec);
        if (c) c.textContent = n === rs.length ? `${rs.length}` : `${n} of ${rs.length}`;
        sec.hidden = n === 0;
        shown += n;
      });
      const count = $('[data-count]', bar);
      if (count) count.textContent = shown === rows.length ? `${shown} patches` : `${shown} of ${rows.length} patches`;
      const empty = $('[data-empty]', root);
      if (empty) empty.hidden = shown > 0;
    };
    bar.addEventListener('click', e => {
      const b = e.target.closest('[data-box] button');
      if (!b) return;
      const box = b.closest('[data-box]').dataset.box;
      const v = b.dataset[box];
      const set = chosen[box] || (chosen[box] = new Set());
      set.has(v) ? set.delete(v) : set.add(v);
      b.setAttribute('aria-pressed', String(set.has(v)));
      apply();
    });
    const input = $('input[data-q]', bar);
    if (input) input.addEventListener('input', () => { q = input.value.trim().toLowerCase(); apply(); });
  });

  /* Flavours: a set box opens its details (from the <template> it names).
     Where the pointer is fine and can hover, a popover beside the box,
     which also opens on hover and stays open while the pointer moves into
     it; elsewhere a sheet from the bottom. Escape or a click outside closes
     either (the Popover API's light dismiss), and focus returns to the box.
     Without the Popover API the boxes stay links to the sections below. */
  const fine = matchMedia('(hover: hover) and (pointer: fine)');
  const canPop = typeof HTMLElement !== 'undefined' && HTMLElement.prototype.hasOwnProperty('popover');
  let pop = null, current = null, mode = null, openT = 0, closeT = 0;
  function closeSet() { if (pop && pop.matches(':popover-open')) pop.hidePopover(); }
  function ensurePop() {
    if (pop) return pop;
    pop = document.createElement('div');
    pop.id = 'setpop';
    pop.className = 'setpop';
    pop.setAttribute('popover', 'auto');
    pop.setAttribute('role', 'dialog');
    pop.innerHTML = '<button type="button" class="x" aria-label="Close">\u00d7</button><div class="body"></div>';
    document.body.appendChild(pop);
    $('.x', pop).addEventListener('click', () => { pop.hidePopover(); });
    pop.addEventListener('toggle', e => {
      if (e.newState !== 'closed' || !current) return;
      const box = current;
      box.setAttribute('aria-expanded', 'false');
      current = null;
      if (mode !== 'hover') box.focus();
    });
    pop.addEventListener('mouseenter', () => clearTimeout(closeT));
    pop.addEventListener('mouseleave', () => { if (mode === 'hover') closeT = setTimeout(closeSet, 250); });
    return pop;
  }
  function place(box) {
    const r = box.getBoundingClientRect(), w = pop.offsetWidth, h = pop.offsetHeight;
    let left = Math.min(Math.max(16, r.left), innerWidth - w - 16);
    let top = r.bottom + 8;
    if (top + h > innerHeight - 8 && r.top - h - 8 > 8) top = r.top - h - 8;
    pop.style.left = `${left}px`;
    pop.style.top = `${Math.max(8, Math.min(top, innerHeight - h - 8))}px`;
  }
  function openSet(box, how) {
    const tpl = document.getElementById(box.dataset.set);
    if (!tpl) return;
    ensurePop();
    if (current && current !== box) current.setAttribute('aria-expanded', 'false');
    const body = $('.body', pop);
    body.replaceChildren(tpl.content.cloneNode(true));
    const h = $('h3', body);
    if (h) { h.id = 'setpop-title'; pop.setAttribute('aria-labelledby', 'setpop-title'); }
    mode = how;
    pop.classList.toggle('sheet', how === 'sheet');
    current = box;
    box.setAttribute('aria-expanded', 'true');
    if (!pop.matches(':popover-open')) pop.showPopover();
    if (how === 'sheet') { pop.style.left = ''; pop.style.top = ''; } else place(box);
    if (how !== 'hover') { const list = $('.setlist', pop) || $('.x', pop); list.focus({ preventScroll: true }); }
  }
  if (canPop) {
    $$('.fgrid a.layer[data-set]').forEach(box => {
      box.setAttribute('role', 'button');
      box.setAttribute('aria-expanded', 'false');
      box.setAttribute('aria-controls', 'setpop');
      if (!box.hasAttribute('href')) box.tabIndex = 0;
      box.addEventListener('click', e => {
        e.preventDefault();
        clearTimeout(openT);
        if (current === box && mode !== 'hover') { closeSet(); return; }
        openSet(box, fine.matches ? 'click' : 'sheet');
      });
      box.addEventListener('keydown', e => { if (e.key === ' ') { e.preventDefault(); box.click(); } });
      box.addEventListener('mouseenter', () => {
        if (!fine.matches || (current && mode !== 'hover')) return;
        clearTimeout(closeT);
        openT = setTimeout(() => openSet(box, 'hover'), 150);
      });
      box.addEventListener('mouseleave', () => {
        clearTimeout(openT);
        if (mode === 'hover' && current === box) closeT = setTimeout(closeSet, 250);
      });
    });
  }

  wireCopy();
})();
