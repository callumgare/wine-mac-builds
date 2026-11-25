#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""tests/site/check.py [--releases <file>] [--site <dir>] [--keep <dir>]

Generates the website from made-up releases (tests/site/releases.json, drafts
included) with tools/site.py, without the network, and checks what it wrote:

  - every page is well formed: a doctype, balanced tags, unique ids, one h1,
    and every href="#…", for, aria-controls (and the other id references)
    names an element on the page
  - nothing the Content-Security-Policy would block: no inline scripts or
    <style>, no style= or on…= attributes
  - links and assets on the site resolve (to a file, a folder's index.html,
    the trailing-slash form Cloudflare adds, or a _redirects line), and a
    #fragment on another page names an element there
  - every loaded resource (src, stylesheets, preloads, icons, CSS url() and
    @import, fetches in scripts) is on the site itself; links to other sites
    are fine
  - _headers keeps default-src 'self' and long caching for /static/, and
    every file under static/ has a content hash in its name
  - _redirects stays under Cloudflare's limits, and the JSON parses

--site checks a site generated already (say, from the real releases) instead.
--keep writes the generated site there, to look at.

Still to come with the pages that need them (the plan's "Checks"):
builds.json against tests/site/builds.keys.json, every SHA256SUMS file on its
build's page, and each build's pasted-name page and download redirects.

Python 3.11+, standard library only.
"""

import argparse
import html.parser
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from sitegen import markup  # noqa: E402

SITE_HOST = "wine-mac-builds.cals.cafe"
VOID = markup.VOID
# Attributes whose value is one or more ids on the same page.
IDREF_ATTRS = {"for": False, "aria-controls": True, "aria-labelledby": True, "aria-describedby": True,
               "aria-owns": True, "aria-activedescendant": False, "popovertarget": False, "list": False,
               "form": False, "headers": True, "commandfor": False}
# Elements and attributes that make the browser load something.
LOADS = {("script", "src"), ("img", "src"), ("img", "srcset"), ("source", "src"), ("source", "srcset"),
         ("video", "src"), ("video", "poster"), ("audio", "src"), ("track", "src"), ("iframe", "src"),
         ("embed", "src"), ("object", "data"), ("input", "src"), ("image", "href"), ("use", "href")}
LOAD_RELS = {"stylesheet", "preload", "modulepreload", "prefetch", "icon", "apple-touch-icon",
             "mask-icon", "manifest"}


class Page(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.problems = []
        self.stack = []
        self.ids = []
        self.h1 = 0
        self.idrefs = []      # (attr, id)
        self.links = []       # hrefs of a/area/link-not-loading
        self.loads = []       # URLs the browser loads
        self.doctype = False
        self.in_script = False

    def problem(self, msg):
        line, col = self.getpos()
        self.problems.append(f"line {line}: {msg}")

    def handle_decl(self, decl):
        if decl.lower() == "doctype html":
            self.doctype = True

    def _attrs(self, tag, attrs):
        a = {}
        for k, v in attrs:
            if k in a:
                self.problem(f"<{tag}> has {k}= twice")
            a[k] = v if v is not None else ""
        if "id" in a:
            self.ids.append(a["id"])
        for k, v in a.items():
            if k == "style":
                self.problem(f"<{tag} style=…>: blocked by the CSP; use a class")
            if k.startswith("on"):
                self.problem(f"<{tag} {k}=…>: blocked by the CSP; use site.js")
            if k in IDREF_ATTRS:
                for ref in (v.split() if IDREF_ATTRS[k] else [v]):
                    self.idrefs.append((f"{tag} {k}", ref))
            if (tag, k) in LOADS:
                urls = [u.strip().split()[0] for u in v.split(",") if u.strip()] if k == "srcset" else [v]
                self.loads += [(f"<{tag} {k}>", u) for u in urls]
        if tag == "link":
            rels = set(a.get("rel", "").lower().split())
            if rels & LOAD_RELS:
                self.loads.append((f"<link rel={a.get('rel')}>", a.get("href", "")))
            else:
                self.links.append((f"<link rel={a.get('rel')}>", a.get("href", "")))
        if tag in ("a", "area") and "href" in a:
            self.links.append((f"<{tag}>", a["href"]))
        if tag == "script" and "src" not in a:
            self.problem("inline <script>: blocked by the CSP; use site.js")
        if tag == "style":
            self.problem("<style>: blocked by the CSP; use site.css")
        if tag == "meta" and a.get("http-equiv", "").lower() == "refresh":
            m = re.search(r"url=(.+)$", a.get("content", ""), re.I)
            if m:
                self.links.append(("<meta refresh>", m[1].strip("'\" ")))
        return a

    def handle_starttag(self, tag, attrs):
        self._attrs(tag, attrs)
        if tag == "h1":
            self.h1 += 1
        if tag not in VOID:
            self.stack.append((tag, self.getpos()[0]))

    def handle_startendtag(self, tag, attrs):
        self._attrs(tag, attrs)
        if tag not in VOID and not any(t in ("svg", "math") for t, _ in self.stack):
            self.problem(f"<{tag}/>: only void elements and SVG may close themselves")

    def handle_endtag(self, tag):
        if tag in VOID:
            self.problem(f"</{tag}>: a void element has no end tag")
            return
        if not self.stack:
            self.problem(f"</{tag}> with nothing open")
            return
        top, line = self.stack[-1]
        if top != tag:
            self.problem(f"</{tag}> closes <{top}> (opened on line {line})")
            if any(t == tag for t, _ in self.stack):
                while self.stack and self.stack[-1][0] != tag:
                    self.stack.pop()
                self.stack.pop()
            return
        self.stack.pop()

    def finish(self):
        self.close()
        if not self.doctype:
            self.problems.append("no <!doctype html>")
        for tag, line in self.stack:
            self.problems.append(f"<{tag}> opened on line {line} is never closed")
        seen = set()
        for i in self.ids:
            if i in seen:
                self.problems.append(f"id {i!r} is used more than once")
            seen.add(i)
        if self.h1 != 1:
            self.problems.append(f"{self.h1} <h1> elements, not one")
        for what, ref in self.idrefs:
            if ref not in seen:
                self.problems.append(f"{what}={ref!r}: no element has that id")
        for what, href in self.links:
            if href.startswith("#") and href != "#" and urllib.parse.unquote(href[1:]) not in seen:
                self.problems.append(f"{what} href={href!r}: no element has that id")


class Checker:
    def __init__(self, site):
        self.site = pathlib.Path(site)
        self.problems = []
        self.pages = {}
        self.redirects = set()

    def problem(self, where, msg):
        self.problems.append(f"{where}: {msg}")

    def url_of(self, p):
        rel = p.relative_to(self.site).as_posix()
        if rel == "index.html":
            return "/"
        if rel.endswith("/index.html"):
            return "/" + rel[:-len("index.html")]
        return "/" + rel

    def same_origin(self, url):
        u = urllib.parse.urlsplit(url)
        if not u.scheme and not u.netloc:
            return True
        return u.scheme == "https" and u.netloc == SITE_HOST

    def resolve(self, base, url):
        """The file a same-origin URL is served from, or None. A redirect
        source resolves to the string "redirect"."""
        u = urllib.parse.urlsplit(urllib.parse.urljoin(f"https://{SITE_HOST}{base}", url))
        path = urllib.parse.unquote(u.path)
        if path in self.redirects or path.rstrip("/") in self.redirects:
            return "redirect"
        rel = path.lstrip("/")
        cands = [rel + "index.html"] if path.endswith("/") else [rel, rel + ".html", rel + "/index.html"]
        for c in cands:
            f = self.site / c
            if c and f.is_file():
                return f
        return None

    def check_url(self, where, base, what, url, load):
        if not url:
            self.problem(where, f"{what} has an empty URL")
            return
        if url.startswith("#"):
            return  # checked within the page
        scheme = urllib.parse.urlsplit(url).scheme
        if scheme in ("mailto", "data") and not load:
            return
        if not self.same_origin(url):
            if load:
                self.problem(where, f"{what} loads {url}, which isn't on the site")
            return
        target = self.resolve(base, url)
        if target is None:
            self.problem(where, f"{what} {url}: nothing on the site there")
            return
        frag = urllib.parse.urlsplit(url).fragment
        if frag and target != "redirect" and target.suffix == ".html":
            page = self.pages.get(target)
            if page and urllib.parse.unquote(frag) not in page.ids:
                self.problem(where, f"{what} {url}: {target.relative_to(self.site)} has no id {frag!r}")

    def run(self):
        if not (self.site / "index.html").is_file():
            self.problem(str(self.site), "no index.html: not a generated site")
            return self.problems
        red = self.site / "_redirects"
        lines = [ln for ln in red.read_text().splitlines() if ln.strip() and not ln.startswith("#")] if red.exists() else []
        for ln in lines:
            parts = ln.split()
            if len(parts) < 2:
                self.problem("_redirects", f"line {ln!r} has no target")
                continue
            self.redirects.add(parts[0])
        dynamic = [ln for ln in lines if "*" in ln.split()[0] or ":" in ln.split()[0]]
        if len(lines) - len(dynamic) > 2000 or len(dynamic) > 100:
            self.problem("_redirects", f"{len(lines) - len(dynamic)} static and {len(dynamic)} dynamic lines; "
                                       "Cloudflare allows 2,000 and 100")

        for p in sorted(self.site.rglob("*.html")):
            page = Page()
            page.feed(p.read_text())
            page.finish()
            self.pages[p] = page
            for msg in page.problems:
                self.problem(p.relative_to(self.site).as_posix(), msg)
        for p, page in self.pages.items():
            where, base = p.relative_to(self.site).as_posix(), self.url_of(p)
            for what, url in page.loads:
                self.check_url(where, base, what, url, load=True)
            for what, url in page.links:
                self.check_url(where, base, what, url, load=False)
        if not (self.site / "404.html").is_file():
            self.problem("404.html", "missing")

        for p in sorted(self.site.rglob("*.css")):
            where, base = p.relative_to(self.site).as_posix(), self.url_of(p)
            css = re.sub(r"/\*.*?\*/", "", p.read_text(), flags=re.S)
            refs = re.findall(r"url\(\s*(['\"]?)(.*?)\1\s*\)", css)
            refs += [("", m) for m in re.findall(r"@import\s+['\"]([^'\"]+)['\"]", css)]
            for _, url in refs:
                if url.startswith("data:"):
                    continue
                self.check_url(where, base, "url()", url, load=True)

        for p in sorted(self.site.rglob("*.js")):
            where, base = p.relative_to(self.site).as_posix(), self.url_of(p)
            js = p.read_text()
            for m in re.finditer(r"""['"`](https?://[^'"`\s]*)""", js):
                if not m[1].startswith("http://www.w3.org/") and not self.same_origin(m[1]):
                    self.problem(where, f"names {m[1]}, which isn't on the site")
            for m in re.finditer(r"\b(fetch|import|importScripts|sendBeacon|EventSource|WebSocket|Worker|"
                                 r"SharedWorker|XMLHttpRequest)\s*\(\s*([^),]*)", js):
                arg = m[2].strip()
                lit = re.fullmatch(r"""(['"`])([^'"`$]*)\1""", arg)
                if m[1] == "XMLHttpRequest" or not lit:
                    self.problem(where, f"{m[1]}({arg}): use a literal same-site URL, so this check can see it")
                else:
                    self.check_url(where, base, f"{m[1]}()", lit[2], load=True)

        hdr = self.site / "_headers"
        text = hdr.read_text() if hdr.exists() else ""
        csp = re.search(r"Content-Security-Policy:\s*(.+)", text)
        if not csp or "default-src 'self'" not in csp[1]:
            self.problem("_headers", "no Content-Security-Policy with default-src 'self'")
        else:
            for directive in csp[1].split(";"):
                for src in directive.split()[1:]:
                    if src not in ("'self'", "'none'"):
                        self.problem("_headers", f"the CSP allows {src}; nothing may load from other sites")
        if not re.search(r"^/static/\*\n(?:[ \t]+.*\n)*?[ \t]+Cache-Control:.*immutable", text, re.M):
            self.problem("_headers", "/static/* isn't cached as immutable")
        for p in sorted((self.site / "static").rglob("*")):
            if p.is_file() and not re.search(r"\.[0-9a-f]{10}(\.[^./]+)?$", p.name):
                self.problem(p.relative_to(self.site).as_posix(), "no content hash in its name, yet cached as immutable")

        for p in sorted(self.site.rglob("*.json")):
            try:
                json.loads(p.read_text())
            except ValueError as e:
                self.problem(p.relative_to(self.site).as_posix(), f"not JSON: {e}")
        return self.problems


def check_repo():
    problems = []
    wr = (ROOT / "wrangler.jsonc").read_text()
    if not re.search(r'"not_found_handling"\s*:\s*"404-page"', wr):
        problems.append('wrangler.jsonc: no "not_found_handling": "404-page" (Cloudflare would never serve 404.html)')
    return problems


def check_markup():
    """The inline copy markup and escaping, on known cases."""
    cases = [
        ("a < b & \"c\"", "a &lt; b &amp; &quot;c&quot;"),
        ("run `wine --version` now", "run <code>wine --version</code> now"),
        ("`<b>` and **bold**", "<code>&lt;b&gt;</code> and <strong>bold</strong>"),
        ("see [the `README`](https://example.org/a?b=1&c=2)",
         'see <a href="https://example.org/a?b=1&amp;c=2">the <code>README</code></a>'),
        ("`a[0](x)` stays code", "<code>a[0](x)</code> stays code"),
        ("`**` isn't bold", "<code>&#42;&#42;</code> isn't bold"),
        ("**[link](/builds/)**", '<strong><a href="/builds/">link</a></strong>'),
        ("<script>**x**</script>", "&lt;script&gt;<strong>x</strong>&lt;/script&gt;"),
    ]
    problems = []
    for src, want in cases:
        got = markup.inline(src)
        if got != want:
            problems.append(f"markup.inline({src!r}) = {got!r}, not {want!r}")
    for bad in ("[x](javascript:alert(1))", "[x](data:text/html,hi)", "**a [b** c](/x)"):
        try:
            markup.inline(bad)
            problems.append(f"markup.inline({bad!r}) didn't refuse it")
        except ValueError:
            pass
    if markup.el("a", "<x>", href='"q"') != '<a href="&quot;q&quot;">&lt;x&gt;</a>':
        problems.append("markup.el doesn't escape")
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[1])
    ap.add_argument("--releases", default=str(ROOT / "tests/site/releases.json"),
                    help="made-up releases to generate from (default: tests/site/releases.json)")
    ap.add_argument("--site", help="check this generated site instead of generating one")
    ap.add_argument("--keep", help="generate into this folder and leave it there")
    args = ap.parse_args()
    problems = check_markup() + check_repo()
    with tempfile.TemporaryDirectory() as tmp:
        site = args.site
        if not site:
            site = args.keep or str(pathlib.Path(tmp) / "site")
            r = subprocess.run([sys.executable, str(ROOT / "tools/site.py"), "--out", site,
                                "--releases", args.releases, "--include-drafts"],
                               capture_output=True, text=True)
            if r.returncode:
                sys.exit(f"tools/site.py failed:\n{r.stdout}{r.stderr}")
        problems += Checker(site).run()
    for p in problems:
        print(p)
    if problems:
        sys.exit(f"{len(problems)} problems")
    print("site checks passed")


if __name__ == "__main__":
    main()
