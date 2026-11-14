# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Flavour "macfixes": WineHQ's release plus the changes Sikarugir's engine of
# the same version carries, rebuilt one documented patch at a time
# (patches/macfixes/<version>/; plan.md section 5). Built like "wine": same
# configure options, add-ons and bundled libraries, so the patches are the
# only difference.

source "$WMB_ROOT/flavours/wine.sh"

FLAVOUR_DESCRIPTION="WineHQ's release with the macOS changes Sikarugir's engines carry"
