#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Flavours: macfixes
# Timeout: 30
# Guards: 0020 gfx-winemac-d3dmetal-macdrv-functions
#
# D3DMetal and DXMT draw into Wine's windows through winemac's Metal views:
# they look up the table of functions winemac.so exports as
# macdrv_functions (with dlsym), and without it they can't present
# anything.
#
# Passes when winemac.so exports the data symbol macdrv_functions. Skipped
# when nm (from the Xcode command line tools) isn't there to look.

so="$T/lib/wine/x86_64-unix/winemac.so"
if ! syms="$(nm -gU "$so" 2>/dev/null)"; then
    echo "macdrv-functions: skipped: nm can't read $so (no Xcode command line tools?)"
    exit 77
fi
if grep -q -E ' [DS] _macdrv_functions$' <<< "$syms"; then
    echo "macdrv-functions: winemac.so exports macdrv_functions"
    echo "macdrv-functions: ok"
    exit 0
fi
echo "macdrv-functions: winemac.so doesn't export macdrv_functions"
echo "macdrv-functions: FAILED"
exit 1
