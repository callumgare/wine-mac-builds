<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# Glossary

The terms this project uses in its code, documentation, JSON and website.
Use these words with these meanings, and add a term here when a new one
comes up.

## What gets built

**WineHQ release.** A version of Wine as WineHQ publishes it: a signed
source tarball such as `wine-11.21.tar.xz`.

**Wine version.** The version number of a WineHQ release: `10.0`, `11.21`,
`11.0-rc2`.

**Channel.** What kind of WineHQ release a version is, from its number:

- **stable:** `X.0` and `X.0.Y`
- **development:** `X.Y`
- **release candidate:** `X.0-rcN`

**Patch.** One `.patch` file with a header in the format of
[patches/README.md](../patches/README.md). It says what the patch changes,
what kind of change it is and where it came from.

**Patch set.** A named group of patches with one purpose, applied as a
whole. A patch set can be part of more than one flavour. A patch set
never has the same name as a flavour.

**Flavour.** A source plus the patch sets applied to it. Flavours built
from the same source are configured and packaged the same way, so their
patch sets are the only difference between them.
`wine` is WineHQ's release with only what's needed to build and package
it; other flavours have their own names (`macfixes`). Each flavour is
defined in `flavours/`.

## What gets published

**Build.** One build of a flavour and Wine version, numbered from 1:
`wine-macfixes-11.21-build1`. That is the build's **build name**. The
plain `wine` flavour leaves its name out: `wine-11.21-build1`. A build is
never changed once published; a fix is a new build.

**Build number.** The `buildN` part. It goes up by one for every new build
of the same flavour and Wine version.

**Platform.** The kind of Mac a build runs on: `macos-x86_64`.

**Build ID.** A build name plus a platform:
`wine-macfixes-11.21-build1-macos-x86_64`. Every file of a build starts
with its build ID, and so does its release's tag.

**Release.** The GitHub release that publishes one build ID's files. It is
how the files are published, not a separate thing to choose between: the
website and the JSON list builds.

**Release files.** The files in a release: the build archive (`.tar.xz`),
debug symbols (`-debug.tar.xz`), tests (`-tests.tar.xz`), corresponding
source (`-sources.tar.xz`), the SBOM (`.spdx.json`), BUILD-INFO
(`.build-info.json`) and checksums (`.SHA256SUMS`).

**BUILD-INFO.json.** The record of what went into a build: source, patches
with their headers, toolchain, environment, configure options and
components. It is at the top of the build archive, and it is also a
release file (`<build ID>.build-info.json`).

**Buildless name.** A build name without its build number:
`wine-macfixes-11.21`. Its website page, JSON and download URLs always
point at the latest build of that flavour and Wine version.

**Latest build.** The highest-numbered build of a flavour and Wine version
that isn't experimental or withdrawn.

## A build's status

**Draft.** A release that hasn't been published. Drafts can be deleted, and
nothing outside the project should depend on one.

**Experimental.** A build whose name ends in `-experimental`. It may be
removed; every other published build is kept.

**Withdrawn.** A published build with a known problem. It stays
downloadable, and the website says why it was withdrawn. It is never the
latest build.

**Superseded.** A build with a higher-numbered build of the same flavour
and Wine version after it.

## How builds are made

**Inputs.** The pinned sources and ordered patch list for one flavour and
Wine version: `inputs/<flavour>/<version>.sh`.

**Toolchain.** The compilers and libraries a build is made with,
identified by a date-based ID (`20261115`) and defined in
`toolchains/<id>.sh`.

**Recipe.** This repository at the commit a build was made from.
