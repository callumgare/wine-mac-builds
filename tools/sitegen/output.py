# SPDX-License-Identifier: LGPL-2.1-or-later
"""Writing the site: static files with content-hashed names, pages, and
_headers.

  site = Site(out, repo="owner/name", site_url="https://…")
  site.asset("site.css")          -> "/static/site.1a2b3c4d5e.css"
  site.page("/builds/", title="Builds", body=…, nav="builds")
  site.text("v1/builds.json", …)  any other file, by its path in the site
  site.finish()                   writes _headers

A page's path is its URL: "/builds/" is written to builds/index.html, and a
path ending in ".html" (/404.html) to that file. Every URL the pages use is
absolute (/static/…), since the 404 page is served at any depth.
"""

import hashlib
import pathlib
import re
import shutil

from . import frame

STATIC = pathlib.Path(__file__).resolve().parent / "static"

# Served for a year without revalidating: every name under /static/ carries
# its content's hash, so a changed file is a new URL.
HEADERS = """\
/*
  Content-Security-Policy: default-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'; object-src 'none'
  X-Content-Type-Options: nosniff
  Referrer-Policy: strict-origin-when-cross-origin
/static/*
  Cache-Control: public, max-age=31536000, immutable
"""


def hashed(name, data):
    stem, dot, ext = name.rpartition(".")
    return f"{stem}.{hashlib.sha256(data).hexdigest()[:10]}.{ext}" if dot else f"{name}.{hashlib.sha256(data).hexdigest()[:10]}"


class Site:
    def __init__(self, out, *, repo, site_url):
        self.out = pathlib.Path(out)
        self.repo = repo
        self.repo_url = f"https://github.com/{repo}"
        self.site_url = site_url.rstrip("/")
        self._assets = {}
        self.pages = []
        self._clean()
        self._copy_static()

    def _clean(self):
        # Only a folder this generator made (it writes static/ first, and
        # _redirects) is emptied, so stale hashed files don't pile up.
        if self.out.exists() and any(self.out.iterdir()):
            if not any((self.out / n).exists() for n in ("_redirects", "static")):
                raise SystemExit(f"error: {self.out} isn't empty and isn't a generated site; remove it or choose another --out")
            for p in self.out.iterdir():
                shutil.rmtree(p) if p.is_dir() and not p.is_symlink() else p.unlink()
        self.out.mkdir(parents=True, exist_ok=True)

    def _put(self, rel, data):
        p = self.out / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)

    def _copy_static(self):
        # Fonts and licence texts first: the stylesheet names them.
        for src in sorted((STATIC / "fonts").iterdir()):
            if src.name == "README":
                continue
            data = src.read_bytes()
            name = hashed(src.name, data)
            self._put(f"static/fonts/{name}", data)
            self._assets[f"fonts/{src.name}"] = f"/static/fonts/{name}"

        def font_url(m):
            key = m[2]
            if key not in self._assets:
                raise SystemExit(f"error: site.css names {key}, which isn't in {STATIC / 'fonts'}")
            return f"url({self._assets[key]})"

        css = re.sub(r"url\((['\"]?)(fonts/[^)'\"]+)\1\)", font_url, (STATIC / "site.css").read_text())
        for name, data in (("site.css", css.encode()), ("site.js", (STATIC / "site.js").read_bytes())):
            h = hashed(name, data)
            self._put(f"static/{h}", data)
            self._assets[name] = f"/static/{h}"
        # The favicon keeps a fixed name at the root, where browsers look.
        self._put("favicon.svg", (STATIC / "favicon.svg").read_bytes())
        self._assets["favicon.svg"] = "/favicon.svg"

    def asset(self, name):
        return self._assets[name]

    def url(self, path):
        return self.site_url + path

    def text(self, rel, content):
        self._put(rel, content.encode())

    def page(self, path, *, title, body, nav=None, description=None, canonical=None, head=None):
        """Write one page. nav is the navigation item it belongs to (home,
        flavours, builds, packaging, patches); a page under that item (a build
        page under builds) passes its item too. canonical defaults to the
        page's own URL; False leaves it out."""
        if not path.startswith("/") or not (path.endswith("/") or path.endswith(".html")):
            raise ValueError(f"page path {path!r}: must start with / and end with / or .html")
        if canonical is None:
            canonical = self.url(path)
        html = frame.render(self, path=path, title=title, body=body, nav=nav,
                            description=description, canonical=canonical or None, head=head)
        rel = path.lstrip("/") + ("index.html" if path.endswith("/") else "")
        self._put(rel, html.encode())
        self.pages.append(path)

    def finish(self):
        self.text("_headers", HEADERS)
