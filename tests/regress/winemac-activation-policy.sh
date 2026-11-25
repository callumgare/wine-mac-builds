#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Versions: 11.22
# Timeout: 180
# Guards: winemac-activation-policy
#
# A program run as a helper of an app that embeds Wine shouldn't get a Dock
# icon or take focus. The Mac Driver "ActivationPolicy" value set to
# "accessory" (globally, or for one exe under AppDefaults) makes the driver
# give its process the accessory policy instead of the regular one.
#
# WINEMAC_ACTIVATION_POLICY in the environment overrides the registry, so a
# launcher can set it for the programs it runs without changing the prefix.
#
# Starts notepad (which shows a window) six ways and asks Launch Services
# what kind of app its process became: with nothing set (Foreground), with
# the global value (UIElement), with the value for notepad.exe only
# (UIElement), with the variable alone (UIElement), with the variable set to
# regular over the global value (Foreground), and with the variable set for
# cmd, which starts notepad (UIElement: Wine programs started from Windows
# code inherit it). Needs a session that can reach the window server;
# skips without one.

set -u
export WINEDEBUG=err+all
server="$(dirname "$WINE")/wineserver"
key='HKCU\Software\Wine\Mac Driver'
appkey='HKCU\Software\Wine\AppDefaults\notepad.exe\Mac Driver'

# kind <name> [command...]: start notepad (or the command, which starts it),
# wait for its window, print its application type ("Foreground",
# "UIElement", ...) or nothing if it never got one.
kind() {
    local pid="" type="" i name="$1"
    shift
    [[ $# -gt 0 ]] || set -- notepad
    "$server" -k > /dev/null 2>&1
    "$WINE" "$@" > "$OUT/$name.log" 2>&1 &
    for i in $(seq 1 60); do
        sleep 1
        for pid in $(pgrep -f 'notepad\.exe'); do
            type="$(lsappinfo info -only ApplicationType "$pid" | sed -n 's/^"ApplicationType"="\(.*\)"$/\1/p')"
            [[ $type == Foreground || $type == UIElement ]] && break 2
        done
        type=""
    done
    "$server" -k > /dev/null 2>&1
    wait
    echo "$type"
}

"$WINE" reg delete "$key" /v ActivationPolicy /f > /dev/null 2>&1
"$WINE" reg delete "$appkey" /v ActivationPolicy /f > /dev/null 2>&1

t="$(kind none)"
if [[ -z $t ]]; then
    if grep -a -q "no driver could be loaded" "$OUT/none.log"; then
        echo "winemac-activation-policy: the Mac driver can't start here (no window server); skipped"
        exit 77
    fi
    echo "winemac-activation-policy: notepad never became an app (see $OUT/none.log)"
    exit 1
fi
echo "winemac-activation-policy: nothing set: $t"
fails=0
[[ $t == Foreground ]] || fails=$((fails + 1))

"$WINE" reg add "$key" /v ActivationPolicy /d accessory /f > /dev/null 2>&1
t="$(kind global)"
"$WINE" reg delete "$key" /v ActivationPolicy /f > /dev/null 2>&1
echo "winemac-activation-policy: Mac Driver\\ActivationPolicy=accessory: ${t:-none}"
[[ $t == UIElement ]] || fails=$((fails + 1))

"$WINE" reg add "$appkey" /v ActivationPolicy /d accessory /f > /dev/null 2>&1
t="$(kind app)"
"$WINE" reg delete "$appkey" /v ActivationPolicy /f > /dev/null 2>&1
"$server" -k > /dev/null 2>&1
echo "winemac-activation-policy: AppDefaults\\notepad.exe ActivationPolicy=accessory: ${t:-none}"
[[ $t == UIElement ]] || fails=$((fails + 1))

t="$(WINEMAC_ACTIVATION_POLICY=accessory kind env)"
echo "winemac-activation-policy: WINEMAC_ACTIVATION_POLICY=accessory: ${t:-none}"
[[ $t == UIElement ]] || fails=$((fails + 1))

"$WINE" reg add "$key" /v ActivationPolicy /d accessory /f > /dev/null 2>&1
t="$(WINEMAC_ACTIVATION_POLICY=regular kind envregular)"
"$WINE" reg delete "$key" /v ActivationPolicy /f > /dev/null 2>&1
echo "winemac-activation-policy: WINEMAC_ACTIVATION_POLICY=regular over ActivationPolicy=accessory: ${t:-none}"
[[ $t == Foreground ]] || fails=$((fails + 1))

t="$(WINEMAC_ACTIVATION_POLICY=accessory kind envchild cmd /c start notepad)"
echo "winemac-activation-policy: WINEMAC_ACTIVATION_POLICY=accessory, notepad started by cmd: ${t:-none}"
[[ $t == UIElement ]] || fails=$((fails + 1))

if [[ $fails -gt 0 ]]; then
    echo "winemac-activation-policy: $fails of 6 wrong"
    exit 1
fi
echo "winemac-activation-policy: ok"
