# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Flavour "macfixes": WineHQ's release with fixes and dormant options for
# macOS: what Sikarugir's engines carry, rebuilt one documented patch at a
# time, and fixes of our own. Built like "wine": same configure options,
# add-ons and bundled libraries, so the patch sets are the only difference.

source "$WMB_ROOT/flavours/wine.sh"

FLAVOUR_DESCRIPTION="WineHQ's release with fixes for macOS, and options that do nothing until enabled"

PATCH_SETS=(build packaging version-label essential-fixes backports targeted-fixes improvements options programs)
VERSIONS=(10.0 11.18 11.19 11.20 11.21 11.22)
