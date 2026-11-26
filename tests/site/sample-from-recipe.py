#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""tests/site/sample-from-recipe.py <checkout>: writes tests/site/sample-patches.json
from a checkout of the recipe with patch sets (patches/<base>/<set>/…).

It records what BUILD-INFO would say about each flavour's patches for each
of its versions, using the checkout's own build/patches.py and
build/patch_header.py and the same steps as build/manifest.py's
patch_entry() and patch_set_entry(). tests/site/make-releases.py builds the
made-up releases' BUILD-INFO from it, so the site is tested on real patch
headers. Each distinct copy is stored once:

  {"recipe_commit": "…",
   "flavours": {"<flavour>": {"description", "base", "sets": [{name, summary, range}], "versions"}},
   "copies": {"<file>": <patch entry>},
   "applied": {"<flavour>/<version>": ["<file>", …]},
   "features": {"<flavour>/<version>": [{name, summary, status, patches}]}}

Run it again when the patches change, and commit the result.
"""

import hashlib
import json
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent


def main(checkout):
    root = pathlib.Path(checkout).resolve()
    sys.path.insert(0, str(root / "build"))
    import patch_header  # noqa: E402  (the checkout's)
    import patches as P  # noqa: E402

    def entry(c):
        f = c.file
        h = patch_header.parse(root / f)
        if h.errors:
            sys.exit(f"{f}: {h.errors[0][1]}")
        header = patch_header.to_json(h)
        data = (root / f).read_bytes()
        return {
            "name": c.patch.name, "set": c.patch.set, "number": c.patch.number, "file": f,
            "versions": c.versions, "sha256": hashlib.sha256(data).hexdigest(),
            "diff_sha256": hashlib.sha256(data.split(b"\n\n", 1)[1]).hexdigest(),
            "summary": header.get("summary"), "category": header.get("category"),
            "kinds": header.get("kinds"), "feature": header.get("feature"), "origins": header.get("origins", []), "header": header,
        }

    out = {"recipe_commit": subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], capture_output=True,
                                           text=True, check=True).stdout.strip(),
           "flavours": {}, "copies": {}, "applied": {}, "features": {}}
    known = P.features()
    for name in P.flavours():
        fl = P.flavour(name)
        sets = []
        for s in fl["sets"]:
            info = P.set_info(fl["base"], s)
            lo, hi = info["range"]
            sets.append({"name": s, "summary": info["summary"], "range": f"{lo:04d}-{hi:04d}"})
        out["flavours"][name] = {"description": fl["description"], "base": fl["base"], "sets": sets,
                                 "versions": fl["versions"]}
        for v in fl["versions"]:
            copies = P.resolve(fl["base"], fl["sets"], v)
            for c in copies:
                out["copies"].setdefault(c.file, entry(c))
            out["applied"][f"{name}/{v}"] = [c.file for c in copies]
            # As build/manifest.py's feature_entries() records them.
            applied = [out["copies"][c.file] for c in copies]
            out["features"][f"{name}/{v}"] = [
                {"name": n, "summary": known.get(n, {}).get("summary"),
                 "status": known.get(n, {}).get("status", {}).get(v),
                 "patches": [p["name"] for p in applied if p.get("feature") == n]}
                for n in sorted({p["feature"] for p in applied if p.get("feature")})]
    (HERE / "sample-patches.json").write_text(json.dumps(out, indent=1, sort_keys=True) + "\n")
    print(f"{len(out['copies'])} copies, {len(out['applied'])} flavour versions")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
