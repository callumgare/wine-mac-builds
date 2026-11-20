# App-embedded Mac driver: host protocol

Version 1, draft. Nothing here is implemented yet; this is the design the
driver will be written against. It will change until the first build that
carries the driver, and after that only by raising the version.

## What it's for

Wine's Mac driver (`winemac.drv`) gives each Wine process its own Cocoa
application: its own Dock icon, menu bar and windows. In host mode, the
driver instead hands its windows to another, native application (the host),
which shows them however it likes: as its own windows, as layers inside one
window, scaled, letterboxed, or not at all. The Wine process draws; the host
decides what reaches the screen and sends back input.

Host mode is per process and off by default. With it off, or with no host
listening, the driver behaves exactly like plain `winemac.drv`.

## Turning it on

- `HKCU\Software\Wine\Mac Driver\HostWindows` (`y`/`n`) for every program,
  overridden per program by
  `HKCU\Software\Wine\AppDefaults\<exe>\Mac Driver\HostWindows`.
- `WINEMAC_HOST_SERVICE=<name>`: the bootstrap service name of the host's
  Mach port. Child processes inherit it, so a launcher and the game it
  starts both reach the same host.

When `HostWindows` is on and the look-up succeeds, the process uses host
mode. If the variable is unset, the look-up fails or the handshake fails
(below), it uses the plain driver and logs why.

## Transport

Mach messages, both ways:

- **Driver → host**: `hello` goes to the host's service port, found with
  `bootstrap_look_up`. The host checks the name in with
  `bootstrap_check_in`, one name per host instance. `hello_reply` carries a
  send right to a port the host makes for this connection, and everything
  after `hello` goes there, so the host can tell processes apart.
- **Host → driver**: a receive right the driver creates and sends in
  `hello`. A driver thread receives on it.

Each Wine process connects on its own, with its own pair of ports. Window,
view and surface ids are per connection.

Every message starts with:

```c
struct wmh_header {
    uint32_t kind;     /* message kind */
    uint32_t serial;   /* nonzero for a call that wants a reply; the reply carries it back */
};
```

The bytes of each message (kinds, structs, which messages carry a port) are
in [app-embedded-protocol.h](app-embedded-protocol.h), which the driver and
hosts share. It covers the first part to be built: the handshake, windows,
GDI surfaces, keys, mouse and focus. The other messages below get their
structs when they're implemented.

Messages on one channel arrive in order, so a surface is always registered
before a frame names it and a window exists before it's shown.

Pictures travel as IOSurfaces, sent as Mach ports
(`IOSurfaceCreateMachPort`), never as global IOSurface ids (deprecated, and
readable by any process).

## Handshake

1. The driver sends `hello`: protocol version, pid, the program's exe name,
   and a send right to its receive port.
2. The host answers `hello_reply`: its protocol version and a bitmask of the
   features it supports. A host that won't take the process answers with
   version 0.

If the versions differ, or no reply comes within 10 seconds, the driver
falls back to the plain driver for this process and logs a warning: the
program's windows then appear outside the host, so a fallback should never
be silent. The wait is long because a fully loaded Mac is where Wine has
already run out its wait for Cocoa to start. Features unknown to one side
are off.

The host checks who is connecting: any process in the login session can
look the service name up. It receives `hello` with `MACH_RCV_TRAILER_AUDIT`,
and answers version 0 unless the sender is a Wine process of the Wine it
started: the audit token's euid is the host's own, and the executable of
the token's pid (`proc_pidpath`) lies inside the Wine folder the host
launched. Parentage can't be used: Wine processes are re-parented to
launchd, and Wine rewrites a process's argument area, so its arguments and
environment can't be read either.

## Coordinates

Every position and size in the protocol, in both directions (window and view
frames, `WINDOW_FRAME_CHANGED`, the cursor messages, mouse events, dirty
rects), is in one space: the one the Mac driver already uses between its C
and Cocoa halves. The origin is the top left of the primary display, y grows
downwards, and units are points; `retina_mode` says whether a point is one
pixel or two. Surfaces are in pixels. The host maps this space onto
whatever it shows (its own window, scaled or letterboxed) and maps input
back into it, so scaling needs nothing from Wine.

## Calls and timeouts

A call that needs an answer sets `serial` and waits for the reply with a
timeout (2 seconds to start with). On timeout the driver logs it and uses a
fallback answer (listed below), so a stuck host can't hang the program.

If the host's port dies, the driver posts `APP_QUIT_REQUESTED` (reason:
logoff), so the program is asked to quit rather than left drawing nowhere.

## Surfaces

A surface is registered once with `surface_add` (id, IOSurface port, width,
height, pixel format) and removed with `surface_remove` when its window or
view is resized or goes away.

Each surface has one owner at a time:

- After `surface_add` it belongs to the driver.
- When the driver sends a frame naming it (`window_frame_ready` or
  `view_frame_ready`), it belongs to the host.
- When the host sends `surface_release` for it, it belongs to the driver
  again.

The host releases a surface once something newer replaces it on screen and
the compositor is done with it (`IOSurfaceIsInUse` false, checked after the
transaction that replaced it has committed). `IOSurfaceIsInUse` alone isn't
enough: a surface that has been sent but not yet shown looks free.

Frame messages carry the surface's width and height. A resize races with
frames already sent, so the host drops a frame whose size doesn't match the
surface as registered (and releases the surface).

The driver draws only into surfaces it owns. Three per OpenGL view and two
per GDI window are the starting counts. With none free, a GL view waits
(bounded) and a GDI window draws into memory and copies it into the next
free surface.

**OpenGL.** The driver maps framebuffer 0 of each view to an FBO whose
colour attachment is the current surface (`CGLTexImageIOSurface2D`,
rectangle texture, BGRA) plus a depth/stencil renderbuffer. At present it
calls `glFinish`, sends `view_frame_ready` and moves to the next free
surface. Win32u, opengl32 and wined3d are unchanged. A fence and a waiting
thread may replace `glFinish` later if a program needs it.

**GDI.** Window surfaces are already IOSurfaces in Wine 11
(`dlls/winemac.drv/surface.c`). Each present sends `window_frame_ready` with
the surface and its dirty rect. With two surfaces, the one the driver moves
to still holds the picture from two presents before, so before drawing into
it the driver brings it up to date: it copies in the union of the dirty
rects since that surface was last sent, or the whole picture (cheap at
these sizes). The host always shows a whole surface; the dirty rect is only
a hint.

## Driver → host messages

One-way unless marked *call*. Each corresponds to a function in
`dlls/winemac.drv/macdrv_cocoa.h`.

| Message | From | Notes |
| --- | --- | --- |
| `window_create` | `create_cocoa_window` | id, features, frame, owner, hwnd (for logs). Sent for hidden windows too, so the host knows one is coming |
| `window_destroy` | `destroy_cocoa_window` | |
| `window_features` | `set_cocoa_window_features` | title bar, buttons, resizable, utility, shadow, prevents activation |
| `window_state` | `set_cocoa_window_state` | disabled, no foreground, floating, minimized, maximized |
| `window_title` | `set_cocoa_window_title` | UTF-8 |
| `window_order` | `order_cocoa_window` | above or below another id; whether to activate |
| `window_hide` | `hide_cocoa_window` | |
| `window_frame` | `set_cocoa_window_frame` | the host may answer with `WINDOW_FRAME_CHANGED` if it disagrees |
| `window_parent` | `set_cocoa_parent_window` | owned windows in the same process; owners in another process aren't supported in v1 |
| `window_frame_ready` | `window_set_io_surface` | surface id, its width and height, rect, dirty rect |
| `window_shape`, `window_alpha`, `window_color_key` | `set_window_shape`, `window_shape_changed`, `set_window_alpha`, `window_use_per_pixel_alpha`, `set_window_mask` | |
| `window_focus` | `give_cocoa_window_focus` | |
| `window_min_max` | `set_window_min_max_sizes` | |
| `window_background` | `window_background_color` | |
| `focus_rejected` | `window_rejected_focus` | |
| `view_create`, `view_destroy`, `view_frame`, `view_parent`, `view_hidden` | `create_view`, `dispose_view`, `set_view_frame`, `set_view_superview`, `set_view_hidden` | a GL region of a window: a layer over the window's picture |
| `view_frame_ready` | `flush_opengl_context` | surface id, its width and height |
| `surface_add`, `surface_remove` | | see Surfaces |
| `cursor_set` | `set_cursor` | a system cursor name, or an image (IOSurface or inline ARGB) with its hot spot; cached by id |
| `cursor_warp` | `set_cursor_position` | the host decides whether to move the real pointer |
| `cursor_clip` | `clip_cursor` | rect or none |
| `mouse_capture` | `set_mouse_capture_window` | |
| `hotkey_register`, `hotkey_unregister` | `register_hot_key`, `unregister_hot_key` | the host sends `HOTKEY_PRESS` |
| `display_mode_changed` | `set_display_mode` | only when the real mode changes (`EmulateModeset` off) |
| `beep` | `beep` | |
| `app_icon` | `set_application_icon` | the host may ignore it |
| `retina_mode` | `set_cocoa_retina_mode` | |
| `quit_reply` | `quit_reply` | |
| `window_get_frame` (*call*) | `get_cocoa_window_frame` | fallback: the last frame sent |
| `cursor_get` (*call*) | `get_cursor_position` | fallback: the last position the host reported |
| `view_backing_size` (*call*) | `get_view_backing_size` | fallback: the view's frame in pixels |
| `displays_get` (*call*) | `get_gpus`, `get_adapters`, `get_monitors` | the host's displays: the window's screen, or what it emulates. Fallback: the real displays |
| `ime_active`, `ime_status` (*call*) | `using_input_method`, `get_ime_open_status` | fallback: false |

## Host → driver messages

**Events.** The host sends the events `cocoa_*.m` produces today, with the
same meaning. The receive thread turns each into a `macdrv_event` and posts
it to the process's event queue (the queue takes its own lock and wakes the
Win32 side through its pipe, so no main thread is involved).

- Input: `KEY_PRESS`, `KEY_RELEASE` (Mac key code, modifiers, time),
  `MOUSE_BUTTON`, `MOUSE_MOVED_ABSOLUTE`, `MOUSE_MOVED_RELATIVE`,
  `MOUSE_SCROLL`, `HOTKEY_PRESS`, `KEYBOARD_CHANGED`. Positions are in the
  protocol's space (see Coordinates): the host undoes any scaling it
  applies. `KEYBOARD_CHANGED` is only a nudge: the driver reads the new
  layout itself through Text Input Sources, which work in any process.
- Focus: `WINDOW_GOT_FOCUS`, `WINDOW_LOST_FOCUS`, `APP_DEACTIVATED`,
  `WINDOW_BROUGHT_FORWARD`, `RELEASE_CAPTURE`. There's no `APP_ACTIVATED`:
  activation shows as a window getting focus.
- Windows: `WINDOW_CLOSE_REQUESTED`, `WINDOW_FRAME_CHANGED`,
  `WINDOW_RESIZE_ENDED`, `WINDOW_DRAG_BEGIN`, `WINDOW_DRAG_END`,
  `WINDOW_DID_UNMINIMIZE`, `WINDOW_RESTORE_REQUESTED`,
  `REASSERT_WINDOW_POSITION`.
- App: `APP_QUIT_REQUESTED` (reason), `DISPLAYS_CHANGED` (the answer to
  `displays_get` has changed).

**Queries.** `RESIZE_SIZE`, `RESIZE_START`, `MIN_MAX_INFO` and the four
drag-and-drop queries carry a serial. The driver posts them with a
completion that sends `query_done` (serial, status, and for a resize the
adjusted rect); the receive thread doesn't wait for the answer.

**Replies** to the driver's calls, and `surface_release`.

## Stays in Wine's process

- The clipboard (the pasteboard is system-wide).
- Display modes. With `EmulateModeset` on, Wine never changes the real
  mode; with it off, the host is told (`display_mode_changed`).
- Status items (tray icons).
- Input source lists.
- OpenGL contexts themselves: only where a view's frames go changes.
- `NSApplication` still runs in host mode: the driver's main-thread
  dispatch, event queues, displays, pasteboard and keyboard layouts rely
  on it. The process never orders a window in, so it keeps the prohibited
  activation policy: no Dock icon, menu bar or window of its own.

## Not in version 1

- Metal and Vulkan (`create_metal_device`, the Metal view, swapchain and
  remote layer functions). A process set to use Vulkan stays on the plain
  driver.
- Text input through an input method (`send_keydown_to_input_source`,
  `clear_ime_text`), which needs a text view in the host.
- Windows whose owner is in another process.

## Behaviour hosts need to get right

- Programs often run for seconds with no visible window (title cards,
  launchers), then show one. The host must be active when the first window
  appears and give it key, or the first clicks and keys are lost. Hidden
  windows are announced with `window_create` so it can prepare.
- Crash reporters and consoles (`winedbg`, `conhost`) open windows too, in
  every Wine process, not only the program's. They arrive as ordinary
  windows the host can keep out of the way.

## Measured so far

The surface handoff was tested with a stand-in sender: an x86_64 process
under Rosetta 2 drawing with CGL into three IOSurface-backed FBOs, and a
native arm64 host setting each surface as a layer's contents. At 800x600,
1920x1440 and 2560x1440, paced at 60 fps, 598 to 600 of 600 frames arrived
and were shown in order. From `glFinish` to the host taking the frame: 0.05
ms at the median and 2 to 11 ms at the 99th percentile, on a loaded Mac.
Without the release step, the sender drew over about 11% of frames before
they were shown.

The driver-side mapping of framebuffer 0 to an IOSurface FBO was tested
inside `winemac.drv` on Wine 11.22 with a Direct3D 9 game (through wined3d,
emulated display mode): over 3,500 frames, the right way up, with no
changes outside the Mac driver.
