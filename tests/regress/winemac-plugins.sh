#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Versions: 11.22
# Timeout: 180
# Guards: winemac-plugins
# Fails without on: wine-macfixes-11.22-build8, macOS 26.7, M1 Max, 2026-11-18
#
# The Mac driver loads the libraries named in WINEMAC_PLUGINS before Cocoa
# starts, so an app that embeds Wine can add code to the driver without
# changing winemac.so. Only absolute paths load.
#
# Builds a plugin whose constructor appends a line to a marker file, then
# starts the driver (wineboot -u does) five ways: with WINEMAC_PLUGINS, with
# a missing path before it (logged, skipped, the rest still load), with a
# relative path (refused and logged, from the plugin's own folder), with
# the old "Plugins" registry value (no longer read: nothing loads), and
# with nothing set (nothing loads). Needs Xcode's clang for
# the plugin, and a session that can reach the window server (a GUI login,
# or a build VM's guest agent); skips without either.

set -u
# Through xcrun, which also finds the macOS SDK's headers.
xcrun -f clang > /dev/null 2>&1 || { echo "winemac-plugins: no Xcode clang to build the test plugin; skipped"; exit 77; }

cat > "$OUT/plug.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
__attribute__((constructor)) static void loaded(void)
{
    const char *path = getenv("WMB_PLUGIN_MARK");
    FILE *f = path ? fopen(path, "a") : NULL;
    if (f) { fprintf(f, "loaded in %d\n", (int)getpid()); fclose(f); }
}
EOF
xcrun clang -arch x86_64 -dynamiclib -o "$OUT/plug.dylib" "$OUT/plug.c" ||
    { echo "winemac-plugins: couldn't build the test plugin"; exit 1; }

export WMB_PLUGIN_MARK="$OUT/mark" WINEDEBUG=err+all
unset WINEMAC_PLUGINS
server="$(dirname "$WINE")/wineserver"
key='HKCU\Software\Wine\Mac Driver'

# start <name>: start the driver afresh; the log goes to $OUT/<name>.log.
start() {
    "$server" -k > /dev/null 2>&1
    rm -f "$WMB_PLUGIN_MARK"
    "$WINE" wineboot -u > "$OUT/$1.log" 2>&1
    "$server" -k > /dev/null 2>&1
}
loaded() { [[ -s $WMB_PLUGIN_MARK ]]; }

WINEMAC_PLUGINS="$OUT/plug.dylib" start env
if ! loaded; then
    if grep -a -q "no driver could be loaded" "$OUT/env.log"; then
        echo "winemac-plugins: the Mac driver can't start here (no window server); skipped"
        exit 77
    fi
    echo "winemac-plugins: WINEMAC_PLUGINS: the plugin wasn't loaded (see $OUT/env.log)"
    exit 1
fi
echo "winemac-plugins: WINEMAC_PLUGINS: loaded in $(wc -l < "$WMB_PLUGIN_MARK" | tr -d ' ') processes"

WINEMAC_PLUGINS="/nonexistent/plugin.dylib:$OUT/plug.dylib" start missing
loaded || { echo "winemac-plugins: a missing path stopped the next one loading (see $OUT/missing.log)"; exit 1; }
grep -a -q "couldn't load plugin \"/nonexistent/plugin.dylib\"" "$OUT/missing.log" ||
    { echo "winemac-plugins: a missing path wasn't reported (see $OUT/missing.log)"; exit 1; }
echo "winemac-plugins: a missing path is reported and skipped"

(cd "$OUT" && WINEMAC_PLUGINS="./plug.dylib" start relative)
if loaded; then echo "winemac-plugins: a relative path loaded (see $OUT/relative.log)"; exit 1; fi
grep -a -q "plugin path \"./plug.dylib\" isn't absolute" "$OUT/relative.log" ||
    { echo "winemac-plugins: a relative path wasn't reported (see $OUT/relative.log)"; exit 1; }
echo "winemac-plugins: a relative path is reported and refused"

"$WINE" reg add "$key" /v Plugins /d "$OUT/plug.dylib" /f > "$OUT/reg.log" 2>&1
start registry
"$WINE" reg delete "$key" /v Plugins /f >> "$OUT/reg.log" 2>&1
"$server" -k > /dev/null 2>&1
if loaded; then echo "winemac-plugins: the Plugins registry value loaded a plugin (see $OUT/registry.log)"; exit 1; fi
echo "winemac-plugins: the Plugins registry value isn't read"

start none
if loaded; then echo "winemac-plugins: a plugin loaded with nothing set"; exit 1; fi
echo "winemac-plugins: with nothing set, nothing loads"
