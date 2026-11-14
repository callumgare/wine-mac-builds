<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# Build metadata

What can change about a build after its release is published, one optional
file per build: `metadata/<build name>.toml` (e.g. `wine-11.21-build1.toml`).
A release itself never changes; `tools/site.py` combines the releases with
these files into the website and `index.json`, and the site workflow
publishes them whenever either changes.

```toml
# SPDX-License-Identifier: LGPL-2.1-or-later
changes = "What changed since the previous build (for build2 onwards)."
known_issues = ["Something that doesn't work, and a link to its issue."]
withdrawn = "Why this build shouldn't be used. It stays downloadable."

[mirrors]
"wine-11.21-build1-macos-x86_64.tar.xz" = ["https://archive.org/download/..."]
```

Builds are never deleted (plan.md, "Never-delete enforcement"): a bad build
is marked `withdrawn` here, and a fixed one is published as the next build
number.
