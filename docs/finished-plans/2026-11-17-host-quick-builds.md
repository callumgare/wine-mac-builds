# Host quick builds on the test Mac (2026-11-17)

> **Finished 2026-11-18**; moved here 2026-11-28. `./wmb host quick` runs
> a quick build on the Mac in the user's GUI session (AGENTS.md, "Fixing
> a bug", step 3). It came out of option A below. The open questions were
> settled after the log ends: host builds got their own compiler cache
> (2b17c08), and the smoke test's windows open in the GUI session because
> the whole build runs there as a LaunchAgent. The full host build
> proposed on 2026-11-18 was made and compared later (078591f: the same
> as a VM build apart from BUILD-INFO.json), and CI now makes releases
> that way. Edits for publishing: machine names are replaced with
> descriptions ("the test Mac"), and the external disk's paths and the
> helper scripts on it are described rather than named. The text is
> otherwise as it was.

Part of the build speed-up work (priority 1). Agreed with the user
2026-11-17: quick test builds may run on the test Mac's host instead of in
the kept quick VM. Releases stay "built in a fresh VM".

## Why

- No VM start, repo copy or shutdown (a few minutes per quick build).
- All of the test Mac's cores and memory instead of the VM's share.
- Accelerated OpenGL, so the GL tests run against the same build at once.
- The test Mac's host Xcode is the VM's: Xcode 27.2 (27B52), SDK 27.2.
  Host macOS is 26.7.2, the VM's 26.6.2; the manifest records both.

## Decisions

- The toolchain stays at `/opt/wmb/toolchains/<id>` (MacPorts bakes the
  path in). No switch to `/Users/Shared/wmb` (user, 2026-11-17).
- On the test Mac `/opt/wmb` is a symlink to the external disk (internal
  disk is tight). Created once by the user with sudo.

## Steps

1. User: one-time setup on the test Mac (sudo). Done 2026-11-17 18:54:
   `/opt/wmb` -> a folder on the external disk, writable by the user.
2. Install toolchain 20261115 on the host from the pinned archive
   (`./wmb toolchain 20261115`, archive already in vm-share/cache).
   Done 2026-11-17 19:06 (3.3 GB); toolchain.sh needed a fix to skip sudo
   when the folder is the user's (a75ed97).
3. Check a host quick build works from a path without spaces (the
   external disk's name has one; Wine's configure rejects spaces):
   WMB_WORK_DIR on the external disk through a symlink without a space.
   Check configure/make don't resolve it to the spaced path.
4. `./wmb host quick <flavour> <version>` (or a flag on `wmb quick`): runs
   quick on the host with its own work dir (separate from the quick VM's),
   then build/hostgl.sh on the archive. Detached runs via a helper like
   the one used for VM builds.
5. Compare one quick build host vs VM: same patches, files differ only
   where expected (paths in debug info, macOS version in the manifest).
   Time both.
6. AGENTS.md: when to use host vs VM quick builds. Local notes: the
   /opt/wmb symlink on the test Mac.

## Log

- 2026-11-17 19:20: step 4 done differently. Plain `./wmb quick` already
  builds on the host, so no new command: it now runs hostgl.sh after the
  archive (except in a VM, WMB_LANE=tart; WMB_HOST_GL=0 skips), and
  compile.sh refuses a WMB_WORK_DIR with a space. Not committed yet: test on
  the test Mac after the build running there (19:12–~20:05).

- 2026-11-17 20:40: hostgl.sh passes WMB_REGRESS_ONLY through (used to
  calibrate d3d8-up-draw-speed). Host quick test postponed again: the test
  Mac is running the macfixes 11.22 release build, ~1 h.

- 2026-11-17 21:50: /opt/wmb is now a real folder on the test Mac's
  internal disk (user replaced the symlink; 115 GB free there). Toolchain
  20261115 reinstalled from the archive; clang's resource dir now reads
  /opt/wmb/...; the external disk's copy deleted. Reason: through the
  symlink, host-built PDBs recorded the external disk's path (see the
  main plan, 21:40). The Decisions and step 1 above describe the symlink
  as it was.

- 2026-11-18 02:55: first host quick build started on the test Mac
  (host1): from a checkout on the external disk at a path without a space,
  with the quick VM's download cache, working tree incl. the uncommitted
  wmb/compile.sh/hostgl.sh changes. Compiles everything (first quick
  build of the version on the host).

## Open

- Whether host quick builds should share the compiler cache (paths differ
  from the VM's, so no hits across the two unless paths are made the same).
- Smoke tests open windows in the user's GUI session on the test Mac.

- 2026-11-18: user chose option A (whole host quick build as a LaunchAgent
  in the GUI session). build/hostquick.sh + gui_job in lib.sh (hostgl.sh now
  uses it too). First run hq1 started 10:19 on the test Mac; work in
  ~/Library/Caches/wmb-hostquick (internal disk).
- 2026-11-18, user: if host builds on the test Mac come out byte-identical
  to VM builds, publishable builds could be made there. Next after the
  host quick timing: a full host build (`wmb build` through gui_job) of the
  commit build10 came from (a0f5d50), compared file by file with build10's
  assets. Expected differences to chase: the manifest's macOS version
  (host 26.7.2, VM 26.6.2), any path outside the mapped source dir. If only
  the manifest differs, the user decides whether releases may come from the
  host (the README says "built in a fresh VM").
- 2026-11-18, user: if only the build record differs, releases from the
  host are acceptable and the README's "built in a fresh VM" line may be
  changed.
- 2026-11-18 10:19-10:48: hq1 (first host quick build, compiles all) failed
  at Manifest: staged repo lacked .git (now included). Compile 10:21-~10:40.
  hq2: gdi32 compile error in gdi32-per-program-color-depth v2 (winreg.h).
  hq3 (gdi32-per-program-color-depth v2, gdi32 only changed): 10:42:30
  start, build 10:42:42, smoke passed 10:45:43, checks 10:46:46, host GL
  passed 10:48:44, done 10:48:47 = 6 min 17 s end to end. color-depth (v2)
  passes on hq3, fails on build9.
- 2026-11-18 11:50-12:03: VM quick build of the same change (6 files changed
  in the VM's tree, 1 in the host's: hq2 had already compiled the winemac
  part) took 12 min 23 s end to end (VM start 19 s; in-VM 00:51:02-00:59:43
  plus archive; shutdown 1 min; host GL 1.5 min), against hq3's 6 min 17 s.
  Per step the VM is about 2x slower: bundling 11 s vs 4, run paths/strip
  ~1 min vs 15 s, signing 1 min 23 s vs 26 s, checks 1 min 24 s vs 26 s,
  smoke 2 min 22 s vs 1 min 34 s. The test Mac's load ~1.5 cores of macOS
  daemons during the VM run; quiet during hq3. Committed: 8cdbc56
  (build-speed).
