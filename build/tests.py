#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""The release file <build ID>.tests.json: what a build's tests showed.

    build/tests.py write <work dir> <build ID> <out dir>
        After smoke.sh and regress.sh: gather their results
        (<work dir>/smoke/results.json, <work dir>/regress/results.json) into
        <out dir>/<build ID>.tests.json.
    build/tests.py add-opengl <regress results.json> <tests.json>
        After build/hostgl.sh ran the OpenGL tests on the host (a VM build's
        own run skips them): record that run under "regress_opengl".

BUILD-INFO.json can't hold these: it's written, and archived, before the
tests run. This file is a separate record, uploaded with the release; it
isn't in SHA256SUMS (archive.sh writes that while the tests run). Each
section lists test names; a test with several architectures appears once per
run (`name (i686)`), as regress.sh names them.
"""

import json
import pathlib
import sys

SCHEMA_VERSION = 1


def load(path):
    p = pathlib.Path(path)
    return json.loads(p.read_text()) if p.is_file() else None


def main(argv):
    if len(argv) == 4 and argv[0] == "write":
        work, build_id, out = pathlib.Path(argv[1]), argv[2], pathlib.Path(argv[3])
        smoke = load(work / "smoke" / "results.json")
        regress = load(work / "regress" / "results.json")
        if smoke is None or regress is None:
            sys.exit(f"tests.py: no smoke or regress results in {work}")
        data = {
            "schema_version": SCHEMA_VERSION,
            "build_id": build_id,
            "smoke": smoke,
            "regress": {k: regress[k] for k in ("passed", "skipped", "failed")},
            # Filled in by add-opengl when a VM build's OpenGL tests run on
            # the host afterwards; null when they ran in the build itself.
            "regress_opengl": None,
        }
        out.mkdir(parents=True, exist_ok=True)
        (out / f"{build_id}.tests.json").write_text(json.dumps(data, indent=2) + "\n")
        return 0
    if len(argv) == 3 and argv[0] == "add-opengl":
        results, target = load(argv[1]), pathlib.Path(argv[2])
        data = load(target)
        if results is None or data is None:
            sys.exit(f"tests.py: missing {argv[1]} or {argv[2]}")
        data["regress_opengl"] = {k: results[k] for k in ("passed", "skipped", "failed")}
        target.write_text(json.dumps(data, indent=2) + "\n")
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
