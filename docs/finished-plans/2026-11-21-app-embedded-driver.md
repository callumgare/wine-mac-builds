# App-embedded Mac driver (host mode in winemac.drv)

> **Finished 2026-11-22**; moved here 2026-11-28. Every item Second Chance
> (the host's author) asked for was done and confirmed through its host,
> and the ports to 11.21 and 10.0 passed their tests. Shape/alpha stays
> open, to be done only if a program needs it. The 2026-11-27 entry at the
> end was a host-side bug and changed no driver code. Edits for publishing:
> patch numbers from before the patch-sets renumbering are replaced with
> patch names. Machine and session names are replaced with descriptions
> ("the test Mac", "a Screen Slots slot"). Second Chance's commit hashes,
> its file names, and paths and checksums of test tarballs are left out.
> The text is otherwise as it was.

Started 2026-11-21. Protocol: docs/app-embedded-protocol.md (version 1
draft, 65ce296). Second Chance writes the host (SkiFree in its window is
its first goal) and asked for something to test against: window create,
GDI surfaces, mouse and keys.

## Approach

- One macfixes 11.22 patch (winemac-host-windows, later split if it
  grows), dormant unless HostWindows is on and WINEMAC_HOST_SERVICE
  resolves. Port to other versions only once confirmed.
- Host mode lives at the C/Cocoa boundary (macdrv_cocoa.h): each function
  the protocol covers checks host mode and sends a message instead of
  touching Cocoa. In host mode a window handle is a small C struct
  (host_window) cast to WineWindow*; the C side treats it as opaque.
- A receive thread turns host messages into macdrv_events and posts them
  to the window's WineEventQueue (it has its own lock and pipe).
- start_cocoa_app starts no NSApplication in host mode.

## Wire format

docs/app-embedded-protocol.h: one header the driver and hosts both use
(Swift can import it). Mach messages, msgh_id = kind, inline body
starts with struct wmh_header. Fixed-width types, explicit padding,
static asserts on sizes (x86_64 driver under Rosetta, arm64 host).

## Slices

1. Header + doc update; tell Second Chance. (They can start the host.)
2. Handshake, receive thread, fallback to plain driver (logged).
3. Windows: create/destroy/frame/hide/order/title/features/state/focus,
   window_get_frame call with timeout + fallback.
4. GDI surfaces: surface_add/remove, window_frame_ready, surface_release,
   two surfaces per window, catch-up copy.
5. Input: KEY_PRESS/RELEASE, MOUSE_BUTTON/MOVED_ABSOLUTE/SCROLL, focus,
   WINDOW_CLOSE_REQUESTED, WINDOW_FRAME_CHANGED, APP_QUIT_REQUESTED.
6. A test host of our own (tools/, native Swift): each window as an
   NSWindow whose layer shows the surface; forwards input. Regression
   test drives it headless where possible.
7. Then: cursor, displays_get, views/OpenGL (GL IOSurface probe), queries.

## Testing

- Quick host builds (no console needed for building; the smoke test opens
  windows, so check with Second Chance before a quick build while they
  hold the console) or rebuild winemac.so in the quick tree and swap it
  into a copy of the latest build.
- Test host + notepad / winemine / a GDI test program; then Second
  Chance's host with SkiFree.

## Log
- 2026-11-21: plan written. Nothing implemented before this.
- 2026-11-21: wire header docs/app-embedded-protocol.h (4b908b9), then merged with Second Chance's proposal (10ee998): msgh_id WMH1, wmh_event, wmh_reply status, frame_ready rects in points (content-relative). Second Chance vendors it and writes its host against it. Driver name still open (a separate winemac_appembedded.drv, or host mode in winemac.drv as the doc says).
- 2026-11-21: winemac-host-windows committed (slice 1: handshake, windows, GDI surfaces, input/focus/close/quit). Kept NSApplication (Prohibited policy) instead of doc's no-NSApplication; recorded in header. Tested with tools/wmh-testhost in a LaunchAgent: explorer+notepad connect, Notepad picture right, closes on request, exit 0. Test tarball host1 for Second Chance. Protocol: per-connection host port in hello_reply (a2ceb8a). NEXT: Second Chance's SkiFree result (keys/mouse); then cursor, displays_get, GL views; update design doc for kept NSApplication.
- 2026-11-21: views (GL/D3D) folded into winemac-host-windows (f2fd411); protocol view messages 30-34, WMH_SURFACE_BOTTOM_UP. Bug found+fixed: select_drawable with no view cleared the current GL context (wined3d calls became no-ops). D3D9 test 7.9 fps host vs 8.4 plain build0 (in a slot), with the test Mac loaded. Message coalescing added. host2 tarball to Second Chance. NEXT: Second Chance's White Wolf result; then cursor (set/hide, clip for cnc-ddraw), displays_get, min/max.
- 2026-11-21: Second Chance confirmed views in its host (gltest/d3d9test right; host 18.3 s vs plain 19.9 s for 279 frames). Cursor added to winemac-host-windows (9cec9a4): set (system/image IOSurface/hidden), warp, clip; position from host mouse events; coordinates = C-side space throughout (dropped mac_from_win on frame_ready/view rects). Dedupe parent/clip too. host3 tarball to Second Chance with cursortest.exe. NEXT: displays_get (+ call machinery w/ timeout), min/max sizes; White Wolf by Second Chance in ~4-5 h.
- 2026-11-21: Second Chance confirmed cursor in its host (status recorded). Size limits added (a283502): WINDOW_MIN_MAX message + host query events QUERY_MIN_MAX_INFO/RESIZE_START posted as fire-and-forget macdrv queries (bug on the way: query types dropped by the type-table check). host4 tarball to Second Chance. Asked Second Chance what displays_get must do (fake displays+modes would be big); defer unless needed. NEXT: wait for Second Chance's White Wolf / cnc-ddraw; then decide displays_get, resize-size query.
- 2026-11-21: Second Chance agrees: no displays_get for now. Only foreseen need: the notch (a geometry/work-area tweak on the real display so a notched built-in display shows Wine without its notch strip; a helper in Second Chance's own process does it today). Second Chance will ask when it gets there.
- 2026-11-21 15:30: WINDOW_MINIMIZE/MAXIMIZE_REQUESTED added (protocol + driver, main-thread dispatch since receive thread can't call win32u). Header documents minimize/maximize/fullscreen-in-Space sequence and that clip must confine the real pointer. Test host WMH_TESTHOST_STATES=1 shows them working with Notepad. White Wolf pointer offset was Second Chance's host (constrainFrameRect). Open: GDI white strip 2240-2496 in host mode, black in plain - unexplained. host5 tarball. Order agreed: minimise/fullscreen, then DnD (named drag pasteboard + query replies), IME last.
- 2026-11-21 16:00: drag and drop: protocol drag (1030) / drag_done (81), driver posts plain's drag-drop queries, opens pasteboard by name, macdrv_set_query_done answers host queries (host_serial). Test host WMH_TESTHOST_DROP: Notepad opened the dropped file (1 run). host6 tarball.
- 2026-11-21 17:00: IME in host mode: ime_key call (first driver->host call; host_call infra with 2 s timeout), ime_rect, ime_clear; KEYBOARD_CHANGED from host now handled (was dropped). Tested in a Screen Slots slot with Japanese Romaji + scripted test host: works. host7 tarball.
- 2026-11-21 17:20: Second Chance: real Romaji through its Swift host works (日本 committed). Composition window = imm32 popup (window 3), shown by driver, 4x16 pt w/o CJK font. ime_rect alternation is upstream imm32 (CFS_RECT passes rcArea) - TODO: reproduce in plain mode with real IM in a slot, then imm32 fix in macfixes (use ptCurrentPos).
- 2026-11-21 17:25: imm32-composition-rect-at-current-pos: imm32 CFS_RECT -> composition rect at ptCurrentPos (font height). Plain build0 in a slot: candidates bottom-left (1 run); with fix: below composition at caret (1 run). host8 = host7 + imm32-composition-rect-at-current-pos (both arches).
- 2026-11-21 18:55: White Wolf white strip explained: plain mode's macdrv_set_window_mask (window.c, fullscreen under EmulateModeset) adds a black CAShapeLayer outside the window rect, hiding the surface's 128-px padding (surface 2176 wide, window narrower); host mode skipped it ('not in v1'). Fix written in quick tree surface.c (host mode: flush paints the padding beyond the window rect opaque black; no protocol change). Compiles; untested (waiting for Second Chance's all-clear). Test: tests/regress program from winemac-centre-fullscreen-gdi or White Wolf through wmh-testhost.
- 2026-11-22 02:10: strip fix tested (test host, 1 run each: fullscreen-gdi-centred grey padding on host8 -> black; White Wolf columns 1920-2175 black). Folded into winemac-host-windows. host9 = host8 + this. Second Chance: all bundles pass hosted on host8 (fixes on its side); imm32-composition-rect-at-current-pos confirmed by Second Chance through its host. Open: Japanese conversion through Second Chance's host (Space stops converting after the first conversion; plain converts) - Second Chance asked for a plain-driver trace of NSTextInputClient queries around Space/Return in a slot.
- 2026-11-22 02:10: Japanese commit bug is the plain driver's too (Second Chance's plain 'commit' was only visual). Traced plain winemac in a slot: with Kotoeri's prediction popup / candidate list up, Space -> にほん {0,3} unconverted, Return -> handleEvent 1 with no callback, nothing committed. Key-ups forwarded (real release time) didn't help (1 run); an earlier run with key-up right after key-down worked but had no prediction popup up. Reverted. Next: native test app, synthetic CGEvent vs real NSEvent into handleEvent, with a candidate list up.
- 2026-11-22 02:30: queue from Second Chance's plan, none patched yet; Second Chance keeps its own workarounds until they land:
  - fullscreen-space-click: WineContentView keeps a click in a fullscreen Space from going to the frame view (cocoa_window.m, ~5 lines). Seen: Webmaster's glove cursor turns into an arrow; a click starts a window drag.
  - clip-follows-window: re-clip the cursor when the clipping window moves/resizes (cocoa_app.m/cocoa_window.m). Seen: Waverly Academy under load.
  - desktop-wait: desktop waits up to 120 s or until explorer exits, not a fixed 10 s that never recovers (win32u winstation.c).
  - exit-with-server: Wine processes exit when their wineserver is gone (ntdll unix server.c, kqueue on server pid). Test: wineserver -k9 with a game running.
  - Names map: scaled-gdi-placement = winemac-centre-fullscreen-gdi; color-depth = gdi32-per-program-color-depth (emulated-palette is separate, not done).
  - WMH_FOCUS_ACTIVATE (e38456e) not yet in a host tarball; host10 when there's more.
- 2026-11-22: host10 = host9 + WMH_FOCUS_ACTIVATE (e38456e). Second Chance: every game in its hosted check sample passes on host8. Second Chance's host now keeps NO_FOREGROUND/disabled windows from becoming key/main unless the program focuses them (Iji's 1x1 owner). exit-with-server, desktop-wait, fullscreen-space-click and clip-follows-window become host11 onward.
- 2026-11-22: exit-with-server = ntdll-exit-with-server (kqueue EV_EOF on fd_socket, watcher thread, exit 1). Repro on host10: sleeper survives -k9 (1 run). Fixed tree: 3 of 3 exit; regress server-gone passes there, fails on host10. Full suite (NO_SCREEN) running on the fixed tree.
- 2026-11-22: ntdll-exit-with-server committed (e13bdd9). Full suite on fixed tree 53/0/2. host11 = host10 + ntdll-exit-with-server ntdll.so. Sent to Second Chance with all-clear. Next desktop-wait.
- 2026-11-22: desktop-wait = win32u-desktop-wait (10 s -> 120 s in get_desktop_window). Second Chance's evidence: nine games at once, explorer ~2.5 min; forced bare desktop -> null driver (nodrv_CreateWindow). Test desktop-wait.sh: Graphics=slow,mac with a wineslow.drv sleeping 15 s; fails on host11 (10.1 s, no window), passes fixed (17-18 s). Full suite 54/0/2. Committed ec493c3. host12 = host11 + win32u-desktop-wait win32u.so.
  - Found: hand-swapped .so files lacked packaging's @loader_path/../../ rpath (host1-11). Fixed in host12; AGENTS.md note.
  - fullscreen-space-click (Second Chance's workaround): swizzled -[WineContentView mouseDown:] returning while styleMask has FullScreen; NSThemeFrame handleMouseDown starts the fullscreen drag (arrow). Webmaster, plain, EmulateModeset, native fullscreen Space.
  - clip-follows-window (Second Chance's workaround): re-run clipCursorHandler startClippingCursor: on NSWindowDidResize/DidMove for the clipping window (startClippingCursor: returns early for an equal rect). Waverly Academy, loaded MacBook.
- 2026-11-22: fullscreen-space-click = winemac-fullscreen-space-click (1fb9889): -[WineContentView mouseDown:] drops the event in a fullscreen Space. Slot evidence: nextResponder NSThemeFrame; program still gets down/moves/up. Arrow not observable in a slot (no live cursor; NSCursor.currentSystem always arrow). host13. Second Chance asked the user for console time to check Webmaster's glove.
- 2026-11-22: clip-follows-window = winemac-clip-follows-window (0c0b3d6): WineWindow windowDidResize (before the ignore_windowResize return) -> controller windowFrameDidChange -> confinement handler resets mouseConfinementRect. Slot debug: stale {0,0,400,300} with update off, {10,0,1910,1080} on (1 run each). Pointer not testable in a slot (absolute moves not confined). host14 = all four (exit-with-server, desktop-wait, fullscreen-space-click, clip-follows-window).
  - Test trap: ClipCursor during the activating click gets reset (foreground change, reset=1).
- 2026-11-22: host14 passed Second Chance's full hosted run (disc 36/36, download 6/6, Her 3/3, Steam 11/11, Big Fish owned+trial). SCK Remastered trial flaky (DRM, same on host8/10). Console checks of fullscreen-space-click/clip-follows-window await the user's OK (Second Chance asking).
- 2026-11-22: user: no draft build16 needed; keep using local test builds until the kinks are out.
- 2026-11-22: console checks on host14 passed (user watched): fullscreen-space-click glove stayed; clip-follows-window Waverly pointer reached bottom right (idle Mac). In their Status (834027d).
- 2026-11-22: Second Chance crash: Waverly msiexec /i <203-char Mac path> crashed on the console account only. Cause: MSI_OpenDatabaseW cwd + "\" + path into WCHAR[MAX_PATH] for paths without '\'. Second Chance now passes a Windows path. msi-database-path-length (e304bb1): allocate db->path. Test msi-unix-path fails on host14, passes fixed; full suite 56/0/2. host15 = host14 + msi.dll.

- 2026-11-22: host15 passed Second Chance's disc set 36/36 in 39 min, MSI installs included (with Second Chance's own Windows-path fix, so msi-database-path-length not exercised alone). Second Chance's publishing checklist now says host15 or later. User: leave bug 60185, no bisect. msi-database-path-length Status updated (3b00bf5).
- 2026-11-22: Second Chance priorities. Emulated palette: parked, no reproducer (only symptom was Webmaster on an earlier test build with gdi32-per-program-color-depth v1, fixed by its v2 + krnl386-attach-without-16bit). Protocol gaps in order: (a) displays_get + DISPLAYS_CHANGED, useful but not urgent (would retire Second Chance's notch helper); (b) shape/alpha only if a program needs it; (c) RESIZE_SIZE: leave unsupported; (d) Metal/Vulkan: keep refusing. IME: go ahead in a slot.
- 2026-11-22 12:08: Japanese "commit failure" is not a bug (plain mode). Screen Slots slot, macOS 26.7, Kotoeri Romaji: native NSTextInputClient app, TextEdit and host15 plain Notepad all behave alike: Space highlights the first prediction (にほん), first Return closes the list (text stays marked), second Return commits. Notepad: "nihon Space Return Return" + "nihon Space Down Return Return" -> pbpaste にほん日本 (1 run each). Earlier trace never pressed a second Return. Host mode with a real IM is only testable through Second Chance's host: asked Second Chance to retry with two Returns. AGENTS.md updated.
- 2026-11-22 12:30: Second Chance's hosted IME retest: both sequences commit (imm trace, Notepad Col 6). IME item CLOSED. Second Chance saw candidate list at screen top left in its host: driver sends ime_rect in protocol (virtual screen) space (testhost: notepad frame 12,120 -> ime_rect 16,143); host must map through its window. Header comment clarified (docs + winemac-host-windows copy, comment only). Asked Second Chance to check its firstRectForCharacterRange mapping.
- 2026-11-22 12:40: Second Chance: its host maps ime_rect (identity, windows at protocol frames); only scaled fullscreen was missing, fixed on its side. Top-left list was right next to Notepad's caret. Header comment reworded. Nothing open on IME.
- 2026-11-22 13:30: displays_get = winemac-host-displays (cocoa_host.m host_get_displays, cocoa_display.m macdrv_get_monitors takes host frames/work areas, display.c current mode size from host + listed + set no-op). Testhost WMH_TESTHOST_DISPLAYS(2). Test (1 run each): 1512x945/work 920 seen by program; DISPLAYS_CHANGED -> 1440x900; unsupported -> real, asked once/process. Second Chance's host side: origin below notch, main display first at 0,0. host16 = host15 + winemac-host-displays winemac.so.
- 2026-11-22 13:55: Second Chance: all on 11.22; host mode 11.x only (user 2026-11-17). Ported ntdll-suspend-context-syscall-return, ntdll-exit-with-server and msi-database-path-length to 11.21 + 10.0 (10.0 ntdll-suspend-context-syscall-return rewritten, no leave_handler). Tests fail on 11.21-build9 / 10.0-build7. Not built: needs full builds of 11.21 and 10.0 (test Mac time, coordinate with Second Chance). displays-popup.exe in hosttests for Second Chance's notch check on the dev Mac.
- 2026-11-22 14:05: winemac-host-displays confirmed by Second Chance on the dev Mac (notch, 2 displays, no helper). Plan: all Second Chance items done except optional shape/alpha (only if a program needs it). Remaining: full builds of 11.21/10.0 for the ports (needs user OK re builds + test Mac time).
- 2026-11-22 18:00: rebuilt test host run once (displays-popup): every kind named (window_features, window_parent, focus_rejected etc.), none unknown. Second Chance's host16 run: disc 36/36, download 6/6, Steam 11/11, Her 3/3; Big Fish trial 9/30 so far, owned pending (~1.5 h). Twister failure was a wedged process in a test session, fixed on Second Chance's side. 11.21/10.0 builds wait for Second Chance's all-clear.
- 2026-11-22 18:30: Second Chance all-clear; host16 full hosted run passed (disc 36/36, download 6/6, Her 3/3, Steam 11/11, Big Fish trial 30/30, owned 1/1). In doc + winemac-host-displays Status (e97b29c). Started ./wmb host build macfixes 11.21 then 10.0 (build0, local).
- 2026-11-22 19:20: 11.21-build0 (local) done: server-gone, msi-unix-path (both arches), suspend-context-syscall-return pass; suite 51/0/3. Statuses updated. 10.0 building (niced).
- 2026-11-22 20:15: 10.0-build0 (first): msi-database-path-length pass; ntdll-exit-with-server FAILED 4/4 (watcher stuck in wine_dbg_printf: no TEB on 10.0) -> write() fix a243d7e, swap test 3/3; ntdll-suspend-context-syscall-return 5/6 (1 crash = test race reading TEB, test fixed). Audio smoke hangs were load (build7 also hung; later 5/5 each). Rebuilding 10.0 full (niced).
- 2026-11-22 21:40: 10.0-build0 (a243d7e) done: all three ports pass, suite 51/0/0, audio smoke ok. Statuses 362f328. Second Chance all-clear sent. Ports DONE. Remaining: move plan to docs/finished-plans (asked user).

## Bug from Second Chance (2026-11-27): wrong y after an emulated mode switch

Owned Big Fish White Wolf (host mode, Second Chance's 11.22-macfixes
test builds, in a Screen Slots slot on the test Mac). Display 2560x1440;
game asks for 800x600 (EmulateModeset, x2.4). The window is created at
800x600 BEFORE the mode switch: first WINDOW_FRAME 0,0 800x600. After the
switch the driver sends 0,-840 2560x1440 (-840 = 600 - 1440: a flip
against the emulated height?). The window sits 840 px above the screen;
no fullscreen Space. Controls: the disc edition (window created after the
switch) gets 0,0 2560x1440; the same game with plain windows (no host
mode) is right. Second Chance's integration test "owned Big Fish" fails
on it.
- Diagnosed 2026-11-27: host-side. The driver sent 0,0 800x600 then
  0,0 2560x1440 (correct; also reproduced with tools/wmh-testhost on 11.22).
  Second Chance's host resized its NSWindow keeping the Cocoa origin
  (0,840), so its frame read back as 0,-840. Told Second Chance the fix
  (recompute origin.y = primary height - y - h on every WINDOW_FRAME).
  Protocol doc now says so (main, wmh_window_frame comment). No driver
  change.
