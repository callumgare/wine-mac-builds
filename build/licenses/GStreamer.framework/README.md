<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# Licences GStreamer.framework leaves out

GStreamer.framework 1.28.1 (Sikarugir-App's build, toolchains/20261115.sh)
carries the licences of most of what's in it in
`Versions/1.0/share/licenses`, which a build's `LICENSES/GStreamer.framework`
links to. These parts are built into it without a folder there, so
build/manifest.py copies this folder into each build as
`LICENSES/GStreamer.framework-additions/`.

Each text is from the source the framework was built from, the same files
its registry source copy holds (build/sources.py):

| Folder | Licence | Taken from |
|---|---|---|
| gst-plugins-good-1.0 | LGPL-2.1-or-later | subprojects/gst-plugins-good/COPYING, gstreamer at de557ba3 |
| gst-plugins-ugly-1.0 | LGPL-2.1-or-later | subprojects/gst-plugins-ugly/COPYING, gstreamer at de557ba3 |
| gst-libav-1.0 | LGPL-2.1-or-later | subprojects/gst-libav/COPYING, gstreamer at de557ba3 |
| frei0r-plugins | GPL-2.0-or-later | COPYING, frei0r v2.3.3 (cerbero's recipe URL and checksum) |
| zbar | LGPL-2.1-or-later | COPYING and LICENSE.md, zbar-0.23.93.tar.gz in cerbero-1.28.1.tar.gz |
| glib-networking | LGPL-2.1-or-later with an OpenSSL exception | COPYING and LICENSE_EXCEPTION, glib-networking-2.80.1.tar.xz in cerbero-1.28.1.tar.gz |
| sqlite3 | public domain | the notice in sqlite3.c, sqlite-amalgamation-3490100.zip in cerbero-1.28.1.tar.gz |

Found by comparing the framework's `share/versions.txt` and its files with
`share/licenses` (2026-11-28). Recheck when the framework changes.
