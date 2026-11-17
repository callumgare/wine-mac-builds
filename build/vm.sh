#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# vm.sh [--keep] <wmb command...>
#
# Runs a wmb command (toolchain, build, ...) inside a fresh clone of the
# toolchain's pinned Tart image, then deletes the clone. Needs Apple silicon
# and Tart (https://tart.run).
#
#   ./wmb vm toolchain 20261111 --archive
#   ./wmb vm build wine 11.21
#
# Shared with the VM (WMB_VM_SHARE, default ./vm-share):
#   repo/   a copy of this checkout, refreshed on every run (and copied into
#           the VM, not used from the share)
#   cache/  downloads (by sha256), kept between runs
#   out/    finished artifacts, including toolchain archives
#   logs/   one log per run
#
# With WMB_VM_CCACHE=1 the compiler cache (build/compile.sh) lives on
# ccache.img in the share: an APFS disk image attached to the VM as a second
# disk, so it outlives the VM. Not a folder on the share: virtiofs can serve
# a file's old contents after it changes, and the cache once returned the
# wrong object from there. One VM at a time can use it
# (ccache.lock); a second runs without it.
#
# The build itself runs on the VM's own disk, which is faster than the share.
#
# Environment: TART_HOME (where Tart keeps images), WMB_VM_CPUS,
# WMB_VM_MEMORY_MB, WMB_BUILD_NUMBER (passed into the VM; default 0),
# WMB_VM_CCACHE, WMB_VM_CCACHE_GB (the disk image's size; default 40).

source "$(dirname "$0")/lib.sh"

KEEP=0
if [[ "${1:-}" == "--keep" ]]; then KEEP=1; shift; fi
[[ $# -ge 1 ]] || die "usage: vm.sh [--keep] <wmb command...>"

# Which image: the toolchain's.
case "$1" in
    toolchain) tc_id="${2:?toolchain id}" ;;
    build|compile|package|smoke|regress|manifest|archive)
        load_build_inputs "${2:?flavour}" "${3:?version}"
        tc_id="$TOOLCHAIN" ;;
    *) die "vm: don't know which image '$1' needs" ;;
esac
load_toolchain "$tc_id"

command -v tart >/dev/null || die "tart isn't installed (brew install openai/tools/tart)"
SHARE="${WMB_VM_SHARE:-$WMB_ROOT/vm-share}"
mkdir -p "$SHARE/repo" "$SHARE/cache" "$SHARE/out" "$SHARE/logs"
rsync -a --delete --exclude /vm-share --exclude /work --exclude /out --exclude /.cache \
    "$WMB_ROOT/" "$SHARE/repo/"

VM="wmb-$(date +%Y%m%d-%H%M%S)-$$"
LOG="$SHARE/logs/$VM.log"
CPUS="${WMB_VM_CPUS:-$(( $(sysctl -n hw.ncpu) - 2 ))}"
MEM="${WMB_VM_MEMORY_MB:-$(( $(sysctl -n hw.memsize) / 1048576 * 2 / 3 ))}"

# --- Compiler cache disk -------------------------------------------------------
CCACHE_IMG=""
CCACHE_LOCK="$SHARE/ccache.lock"
CCACHE_VOLUME="wmb-ccache"
take_ccache_lock() {
    if mkdir "$CCACHE_LOCK" 2>/dev/null; then echo $$ > "$CCACHE_LOCK/pid"; return 0; fi
    # A lock whose owner has gone (a killed run) is stale.
    local owner; owner="$(cat "$CCACHE_LOCK/pid" 2>/dev/null || true)"
    if [[ -n "$owner" ]] && ! kill -0 "$owner" 2>/dev/null; then
        echo $$ > "$CCACHE_LOCK/pid"; return 0
    fi
    return 1
}
make_ccache_image() {
    local img="$1" dev
    log "Creating $img (${WMB_VM_CCACHE_GB:-40} GB, sparse, APFS volume $CCACHE_VOLUME)"
    dd if=/dev/zero of="$img.new" bs=1 count=0 seek="${WMB_VM_CCACHE_GB:-40}g" 2>/dev/null
    dev="$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount "$img.new" | awk 'NR==1 {print $1}')"
    diskutil eraseDisk APFS "$CCACHE_VOLUME" GPT "$dev" >/dev/null
    hdiutil detach "$dev" >/dev/null
    mv "$img.new" "$img"
}
if [[ "${WMB_VM_CCACHE:-0}" == 1 ]]; then
    if take_ccache_lock; then
        CCACHE_IMG="$SHARE/ccache.img"
        [[ -f "$CCACHE_IMG" ]] || make_ccache_image "$CCACHE_IMG"
    else
        log "The ccache disk is in use by another VM ($CCACHE_LOCK); building without it"
    fi
fi

cleanup() {
    # `tart stop` cuts the power, losing writes the guest hasn't flushed,
    # which makes a kept VM misleading to debug. Flush and shut down first.
    tart exec "$VM" /bin/sync >/dev/null 2>&1 || true
    tart exec "$VM" sudo shutdown -h now >/dev/null 2>&1 || true
    for _ in $(seq 30); do
        tart list 2>/dev/null | grep -E "[[:space:]]${VM}[[:space:]].*running" >/dev/null || break
        sleep 2
    done
    tart stop "$VM" >/dev/null 2>&1 || true
    if [[ -n "$CCACHE_IMG" ]]; then rm -rf "$CCACHE_LOCK"; fi
    if [[ $KEEP -eq 1 ]]; then
        log "Kept VM $VM (tart run $VM; tart delete $VM when done)"
    else
        tart delete "$VM" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

log "Cloning $TART_IMAGE as $VM ($CPUS CPUs, $MEM MB)"
tart clone "$TART_IMAGE" "$VM"
tart set "$VM" --cpu "$CPUS" --memory "$MEM"
tart run "$VM" --no-graphics --dir="wmb:$SHARE" ${CCACHE_IMG:+--disk="$CCACHE_IMG"} > "$SHARE/logs/$VM.tart.log" 2>&1 &
tart ip "$VM" --wait 300 >/dev/null || die "VM didn't come up"
# The guest agent starts a moment after networking.
for _ in $(seq 60); do tart exec "$VM" true 2>/dev/null && break; sleep 2; done

GUEST_SHARE="/Volumes/My Shared Files/wmb"
args="$(printf ' %q' "$@")"
log "Running in $VM: wmb$args (log: $LOG)"
# Copy the checkout into the VM over the exec channel. Not via the share:
# virtiofs in the guest can keep serving a file's old contents after the
# host replaces it.
(cd "$SHARE/repo" && COPYFILE_DISABLE=1 tar -cf - .) |
    tart exec -i "$VM" /bin/bash -c 'rm -rf "$HOME/wmb" && mkdir "$HOME/wmb" && tar -xf - -C "$HOME/wmb"'
# Run detached inside the VM, writing the log and exit code to the share.
# A long `tart exec` can drop ("Transport became inactive") during a step
# that prints nothing for a while, which would kill the build; the host
# follows the log file instead.
EXIT_FILE="$SHARE/logs/$VM.exit"
JOB="$SHARE/logs/$VM.job.sh"
rm -f "$EXIT_FILE"; : > "$LOG"
{
    echo '#!/bin/bash'
    echo 'set -uo pipefail'
    printf 'exec > %q 2>&1 < /dev/null\n' "$GUEST_SHARE/logs/$VM.log"
    echo '('
    echo '    set -euo pipefail'
    echo '    cd "$HOME/wmb"'
    printf '    export WMB_CACHE_DIR=%q WMB_OUT_DIR=%q WMB_WORK_DIR="$HOME/work"\n' "$GUEST_SHARE/cache" "$GUEST_SHARE/out"
    printf '    export WMB_LANE=tart GITHUB_ACTIONS=%q WMB_BUILD_NUMBER=%q\n' "${GITHUB_ACTIONS:-}" "${WMB_BUILD_NUMBER:-0}"
    if [[ -n "$CCACHE_IMG" ]]; then
        # macOS mounts the attached disk's volume by itself; its root
        # belongs to root.
        printf '    v=/Volumes/%q\n' "$CCACHE_VOLUME"
        echo '    for _ in $(seq 30); do [[ -d "$v" ]] && break; sleep 2; done'
        echo '    if [[ -d "$v" ]] && sudo -n chown "$(id -un)" "$v"; then export WMB_CCACHE_DIR="$v/ccache"'
        echo '    else echo "The ccache disk did not mount; building without it"; fi'
    fi
    if [[ "$1" != toolchain ]]; then printf '    ./wmb toolchain %q\n' "$tc_id"; fi
    echo "    ./wmb$args"
    echo ')'
    printf 'echo $? > %q\n' "$GUEST_SHARE/logs/$VM.exit"
} > "$JOB"
tart exec "$VM" /bin/bash -c "nohup /bin/bash '$GUEST_SHARE/logs/$VM.job.sh' > /dev/null 2>&1 < /dev/null &"

tail -n +1 -F "$LOG" &
TAIL_PID=$!
until [[ -s "$EXIT_FILE" ]]; do
    tart list 2>/dev/null | grep -E "[[:space:]]${VM}[[:space:]].*running" >/dev/null || { kill $TAIL_PID; die "VM $VM stopped unexpectedly"; }
    sleep 10
done
sleep 2; kill $TAIL_PID 2>/dev/null || true
status="$(cat "$EXIT_FILE")"
[[ "$status" == 0 ]] || die "wmb$args failed in $VM (exit $status); log: $LOG"
log "Done: wmb$args"
