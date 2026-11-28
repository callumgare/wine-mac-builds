# App-embedded Mac driver: host protocol

Version 1, draft. The driver side is implemented in the macfixes 11.22
patches `winemac-host-windows` (`winemac: HostWindows option`) and `winemac-host-displays` (displays), and has
been tested in local test builds with `tools/wmh-testhost` and with Second
Chance's host. No numbered build carries it yet. The protocol may change
until the first build that does, and after that only by raising the
version. The tables below say which messages are implemented.

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
hosts share (`winemac-host-windows` adds a copy to the driver as `wmh_protocol.h`; the two
are kept identical). It defines every implemented message. The planned ones
in the tables below get their structs when they're implemented.

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
rects, `ime_rect`, the displays), is in one space: the one the Mac driver
already uses between its C and Cocoa halves. The origin is the top left of
the primary display as `displays_get` reports it (a host that keeps windows
below a notch puts the origin below it), y grows downwards, and units are
points, or pixels when Wine's RetinaMode is on (the host halves them;
RetinaMode in host mode hasn't been tested). Surfaces are in pixels. The
host maps this space onto whatever it shows (its own window, scaled or
letterboxed) and maps input back into it, so scaling needs nothing from
Wine.

## Calls and timeouts

A call that needs an answer sets `serial` and waits for the reply with a
timeout (2 seconds). On timeout the driver logs it and uses a fallback
answer (listed below), so a stuck host can't hang the program. The
implemented calls are `displays_get` and `ime_key`; `window_get_frame` and
`cursor_get` are defined, but the driver answers them from its own record
(the last frame and pointer position it sent or was told) without asking.

If the host's port dies, or receiving from it fails, the driver posts `APP_QUIT_REQUESTED` (reason:
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

The driver draws only into surfaces it owns, up to three per GDI window and
three per OpenGL view. With none free, a GL view waits (bounded) and a GDI
window draws into memory and copies it into the next free surface.

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
`dlls/winemac.drv/macdrv_cocoa.h`. "Planned" messages aren't in the header
yet; in host mode the driver does nothing for them.

| Message | From | Status | Notes |
| --- | --- | --- | --- |
| `window_create` | `create_cocoa_window` | done | id, features, frame, owner, hwnd (for logs). Sent for hidden windows too, so the host knows one is coming |
| `window_destroy` | `destroy_cocoa_window` | done | |
| `window_features` | `set_cocoa_window_features` | done | title bar, buttons, resizable, utility, shadow, prevents activation |
| `window_state` | `set_cocoa_window_state` | done | disabled, no foreground, floating, minimized, maximized |
| `window_title` | `set_cocoa_window_title` | done | UTF-8 |
| `window_order` | `order_cocoa_window` | done | above or below another id; whether to activate |
| `window_hide` | `hide_cocoa_window` | done | |
| `window_frame` | `set_cocoa_window_frame` | done | the host may answer with `WINDOW_FRAME_CHANGED` if it disagrees |
| `window_parent` | `set_cocoa_parent_window` | done | owned windows in the same process; owners in another process aren't supported in v1 |
| `window_frame_ready` | `window_set_io_surface` | done | surface id, its width and height, rect, dirty rect |
| `window_focus` | `give_cocoa_window_focus` | done | `WMH_FOCUS_ACTIVATE`: also bring the app to the front |
| `window_min_max` | `set_window_min_max_sizes` | done | also the answer to `QUERY_MIN_MAX_INFO` and `QUERY_RESIZE_START` |
| `focus_rejected` | `window_rejected_focus` | done | |
| `view_create`, `view_destroy`, `view_frame`, `view_hidden` | `create_view`, `dispose_view`, `set_view_frame`, `set_view_hidden` | done | an OpenGL region of a window: a layer over the window's picture |
| `view_frame_ready` | `flush_opengl_context` | done | surface id, its width and height |
| `surface_add`, `surface_remove` | | done | see Surfaces |
| `cursor_set` | `set_cursor` | done | hidden, a system cursor name, or an image (IOSurface) with its hot spot |
| `cursor_warp` | `set_cursor_position` | done | the host decides whether to move the real pointer |
| `cursor_clip` | `clip_cursor` | done | rect or none; the host must confine the real pointer |
| `displays_get` (*call*) | `get_monitors`, the current mode | done (`winemac-host-displays`) | the host's displays: the window's screen, or what it emulates. The first is the primary, at the origin. Each becomes a monitor (frame, work area), and its frame's size the current mode, which is also listed and setting it changes nothing. Fallback (no answer, unsupported, or the first not at the origin): the real displays. A host that answered unsupported isn't asked again; one that didn't answer in time is asked again at the next enumeration |
| `ime_key` (*call*) | `send_keydown_to_input_source` | done | the host's input context handles the key and returns the marked and committed text. Fallback: not handled |
| `ime_clear` | `clear_ime_text` | done | discard the marked text |
| `ime_rect` | `SetIMECompositionRect` | done | for the candidate window, in the protocol's space; the host maps it as it maps the window's picture |
| `drag_done` | (dragging destination) | done | the answer to each `drag` step |
| `resize_size_reply` | (`windowWillResize:toSize:`) | done | the answer to `QUERY_RESIZE_SIZE`: its serial and the frame the program's WM_SIZING chose |
| `window_get_frame` (*call*) | `get_cocoa_window_frame` | defined | the driver uses its fallback, the last frame sent or reported, without asking |
| `cursor_get` (*call*) | `get_cursor_position` | defined | the driver uses its fallback, the last position the host reported, without asking |
| `window_shape`, `window_alpha`, `window_color_key` | `set_window_shape`, `window_shape_changed`, `set_window_alpha`, `window_use_per_pixel_alpha`, `set_window_mask` | planned | shaped and layered windows show as opaque rectangles until then |
| `window_background` | `window_background_color` | planned | |
| `view_parent` | `set_view_superview` | planned | views nested in other views |
| `mouse_capture` | `set_mouse_capture_window` | planned | |
| `hotkey_register`, `hotkey_unregister` | `register_hot_key`, `unregister_hot_key` | planned | the host would send `HOTKEY_PRESS` |
| `display_mode_changed` | `set_display_mode` | planned | only when the real mode changes (`EmulateModeset` off) |
| `beep` | `beep` | planned | |
| `app_icon` | `set_application_icon` | planned | the host may ignore it |
| `retina_mode` | `set_cocoa_retina_mode` | planned | |
| `quit_reply` | `quit_reply` | planned | kind and struct are in the header, but the driver doesn't send it yet |
| `view_backing_size` (*call*) | `get_view_backing_size` | planned | fallback: the view's frame in pixels |

## Host → driver messages

**Events.** The host sends the events `cocoa_*.m` produces today, with the
same meaning. The receive thread turns each into a `macdrv_event` and posts
it to the process's event queue (the queue takes its own lock and wakes the
Win32 side through its pipe, so no main thread is involved).

- Input: `KEY_PRESS`, `KEY_RELEASE` (Mac key code, modifiers, time),
  `MOUSE_BUTTON`, `MOUSE_MOVED_ABSOLUTE`, `MOUSE_MOVED_RELATIVE`,
  `MOUSE_SCROLL`, `KEYBOARD_CHANGED` (`HOTKEY_PRESS` is planned, with
  `hotkey_register`). Positions are in the
  protocol's space (see Coordinates): the host undoes any scaling it
  applies. `KEYBOARD_CHANGED` is only a nudge: the driver reads the new
  layout itself through Text Input Sources, which work in any process.
  A host sends it whenever the input source changes: in host mode macOS's
  own notice doesn't reach the driver (it ignores it without a text input
  context of its own).
- Focus: `WINDOW_GOT_FOCUS`, `WINDOW_LOST_FOCUS`, `APP_DEACTIVATED`,
  `WINDOW_BROUGHT_FORWARD`, `RELEASE_CAPTURE`. There's no `APP_ACTIVATED`:
  activation shows as a window getting focus.
- Windows: `WINDOW_CLOSE_REQUESTED`, `WINDOW_FRAME_CHANGED`,
  `WINDOW_RESIZE_ENDED`, `WINDOW_DID_UNMINIMIZE`, `WINDOW_RESTORE_REQUESTED`,
  `REASSERT_WINDOW_POSITION`, `WINDOW_MINIMIZE_REQUESTED`,
  `WINDOW_MAXIMIZE_REQUESTED`. The plain driver handles the user's
  minimize and zoom inside its NSWindow and asks the program by calling
  into win32u; a host sends the two requests instead, and shows the
  program's answer when `window_state` and frames arrive (the header
  gives the sequence, and fullscreen in a Space). `WINDOW_DRAG_BEGIN` and
  `WINDOW_DRAG_END` (the user moving a window by its title bar) are
  planned.
- App: `APP_QUIT_REQUESTED` (reason), `DISPLAYS_CHANGED` (the answer to
  `displays_get` has changed).

**Queries.** `QUERY_MIN_MAX_INFO` and `QUERY_RESIZE_START` are events
the driver answers with ordinary messages (`window_min_max`). Drag and
drop onto a window is a `drag` message per step (enter, update, leave,
drop), each answered with `drag_done` (the drag's serial, accepted, the
operation to show). The host waits for those answers, as the plain
driver's dragging methods do; the driver reads the dragged data from the
drag pasteboard by name. The driver posts all of these to the window's
event queue; its receive thread doesn't wait for the answer.
`QUERY_RESIZE_SIZE` is asked during a live resize, before the host
applies each new size, as the plain driver's `windowWillResize:toSize:`
asks: the event's header serial, the proposed content frame and the edge
being dragged (WMSZ_*). The driver sends the program WM_SIZING, through
which a program keeps a minimum size or an aspect ratio, and answers with
`resize_size_reply` (the serial and the frame to use). The host waits up
to 0.1 s and uses the proposed frame if no answer comes.

**Replies** to the driver's calls, and `surface_release`.

## Stays in Wine's process

- The clipboard (the pasteboard is system-wide).
- Display modes. With `EmulateModeset` on, Wine never changes the real
  mode. With it off, Wine can change the real mode itself; telling the
  host (`display_mode_changed`) is planned. With `winemac-host-displays`, the current mode is
  the host's display's size, and setting that size changes nothing.
- Status items (tray icons).
- Input source lists.
- OpenGL contexts themselves: only where a view's frames go changes.
- `NSApplication` still runs in host mode: the driver's main-thread
  dispatch, event queues, displays, pasteboard and keyboard layouts rely
  on it. The process never orders a window in, so it keeps the prohibited
  activation policy: no Dock icon, menu bar or window of its own.

## Not in version 1

- Metal and Vulkan (`create_metal_device`, the Metal view, swapchain and
  remote layer functions): Vulkan, DXVK and D3DMetal. Host mode refuses to
  attach a Metal view to a host window, so such a program shows nothing in
  the host; use Wine's own Direct3D (wined3d over OpenGL), which host mode
  supports.
- Windows whose owner is in another process.
- The messages marked "planned" in the tables.

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

With the driver implemented (`winemac-host-windows`), a GL test program drew 279 frames in
18.3 s through Second Chance's host, against 19.9 s with the plain driver
(one run each). Second Chance's full hosted integration runs passed on the
local test builds: on host14, disc 36/36, download 6/6, Her downloads 3/3,
Steam 11/11 and Big Fish; on host15, disc 36/36; on host16 (host15 with
`winemac-host-displays`), disc 36/36, download 6/6, Her downloads 3/3, Steam 11/11, Big Fish
trials 30/30 and the one owned Big Fish game (one run each; one Her game
passed on a rerun after a failure in Second Chance's own launcher). `winemac-host-displays`
was checked on a notched MacBook Pro with a second display, one run.

## Testing without a real host

`tools/wmh-testhost` checks in a Mach service, logs every message, saves
each window's picture as a PNG, and can script focus, window states, a
drag, input-method keys and displays (see AGENTS.md for how to run it and
its options). `tools/displays-popup` prints what Wine tells a program about
the displays and opens a popup over the primary monitor.
