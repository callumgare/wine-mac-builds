/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * App-embedded Mac driver: host protocol wire format, version 1 (draft).
 * The design is docs/app-embedded-protocol.md; this file fixes the bytes,
 * and is the one copy: the driver and every host use it as is. Plain C,
 * fixed-width fields, no pointers, no bool, no enums in structs. The sizes
 * are the same on x86_64 (the driver, under Rosetta 2) and arm64; the
 * static asserts at the end check them.
 *
 * Every message is one Mach message:
 *
 *     mach_msg_header_t                    msgh_id = WMH_MSG_ID, always
 *     [mach_msg_body_t + port descriptors] only for the kinds marked PORTS
 *     struct wmh_header                    kind, serial
 *     the kind's payload struct
 *
 * msgh_bits has MACH_MSGH_BITS_COMPLEX set exactly when descriptors follow.
 * A message shorter than its kind's payload is dropped (and logged). A
 * longer one is read up to the fields the receiver knows, so either side can
 * add fields at the end of a payload without a version change. Unknown kinds
 * are ignored; a call of an unknown kind is answered with a WMH_KIND_REPLY
 * whose status is WMH_STATUS_UNSUPPORTED.
 *
 * hello goes to the host's service port. The host answers with a port of
 * its own for this connection (in hello_reply), and the driver sends
 * everything after hello there, so the host knows which process each
 * message comes from. Host -> driver goes to the receive right the driver
 * sends in hello. Each direction is ordered.
 *
 * Ids (window, surface) are chosen by the driver, per connection, never 0,
 * never reused while live. 0 means "none".
 *
 * Positions and sizes are in the protocol's space (see the design doc): the
 * units the Mac driver uses between its C and Cocoa halves, origin at the
 * top left of the primary display, y down, as doubles. They're points
 * unless Wine's RetinaMode is on, when they're pixels (the host halves
 * them). Surface sizes are in pixels.
 */

#ifndef WMH_PROTOCOL_H
#define WMH_PROTOCOL_H

#include <stdint.h>

#define WMH_PROTOCOL_VERSION 1u
#define WMH_MSG_ID           0x574d4831u /* 'WMH1' */

#define WMH_EXE_NAME_MAX     256u        /* bytes of UTF-8 with the NUL; longer is truncated */
#define WMH_TITLE_MAX        512u

typedef uint32_t wmh_window_id;
typedef uint32_t wmh_surface_id;

struct wmh_header {
    uint32_t kind;      /* WMH_KIND_* */
    uint32_t serial;    /* nonzero on a call; its reply carries it back */
};

struct wmh_rect { double x, y, width, height; };

enum {
    /* ---- driver -> host ---- */
    WMH_KIND_HELLO = 1,                 /* PORTS: [0] send right to the driver's receive port */
    WMH_KIND_WINDOW_CREATE = 10,
    WMH_KIND_WINDOW_DESTROY,
    WMH_KIND_WINDOW_FEATURES,
    WMH_KIND_WINDOW_STATE,
    WMH_KIND_WINDOW_TITLE,
    WMH_KIND_WINDOW_ORDER,
    WMH_KIND_WINDOW_HIDE,
    WMH_KIND_WINDOW_FRAME,
    WMH_KIND_WINDOW_PARENT,
    WMH_KIND_WINDOW_FOCUS,
    WMH_KIND_WINDOW_FRAME_READY,
    WMH_KIND_FOCUS_REJECTED,
    WMH_KIND_WINDOW_MIN_MAX,
    WMH_KIND_VIEW_CREATE = 30,          /* struct wmh_view_ref */
    WMH_KIND_VIEW_DESTROY,              /* struct wmh_view_ref */
    WMH_KIND_VIEW_FRAME,
    WMH_KIND_VIEW_HIDDEN,
    WMH_KIND_VIEW_FRAME_READY,
    WMH_KIND_CURSOR_SET = 50,           /* PORTS (image only): [0] the image's IOSurface, moved */
    WMH_KIND_CURSOR_WARP,
    WMH_KIND_CURSOR_CLIP,
    WMH_KIND_SURFACE_ADD = 40,          /* PORTS: [0] the IOSurface (IOSurfaceCreateMachPort), moved */
    WMH_KIND_SURFACE_REMOVE,
    WMH_KIND_WINDOW_GET_FRAME = 60,     /* call: struct wmh_reply_rect */
    WMH_KIND_CURSOR_GET,                /* call: struct wmh_reply_point */
    WMH_KIND_DISPLAYS_GET,              /* call: struct wmh_reply_displays */
    WMH_KIND_IME_KEY,                   /* call: struct wmh_ime_key; reply struct wmh_reply_ime_key */
    WMH_KIND_IME_CLEAR = 70,            /* no payload struct beyond the header */
    WMH_KIND_IME_RECT,                  /* struct wmh_ime_rect */
    WMH_KIND_QUIT_REPLY = 80,
    WMH_KIND_DRAG_DONE,                 /* struct wmh_drag_done: the answer to a drag */

    /* ---- host -> driver ---- */
    WMH_KIND_HELLO_REPLY = 1001,        /* PORTS: [0] send right to the host's port for this connection */
    WMH_KIND_REPLY,                     /* struct wmh_reply, then the call's reply struct */
    WMH_KIND_SURFACE_RELEASE = 1010,
    WMH_KIND_EVENT = 1020,              /* struct wmh_event */
    WMH_KIND_DRAG = 1030,               /* struct wmh_drag; answered with drag_done */
};

/* ---- handshake ---- */

/* hello.features (what the driver can send) and hello_reply.features (the
 * part the host will use). */
#define WMH_FEATURE_GDI_SURFACES  (1u << 0)
#define WMH_FEATURE_INPUT         (1u << 1)
#define WMH_FEATURE_GL_VIEWS      (1u << 2)   /* takes view surfaces */
#define WMH_FEATURE_CURSOR        (1u << 3)   /* takes cursor_set, cursor_warp, cursor_clip */

struct wmh_hello {
    uint32_t protocol_version;          /* WMH_PROTOCOL_VERSION */
    uint32_t features;
    int32_t  pid;                       /* the host checks it against the audit token's */
    uint32_t reserved;
    char     exe_name[WMH_EXE_NAME_MAX]; /* "Game.exe", as AppDefaults names it; NUL-terminated */
};

/* A refusal (version 0) needs no port. */
struct wmh_hello_reply {
    uint32_t protocol_version;          /* 0: the host won't take this process */
    uint32_t features;
    uint32_t reserved[2];
};

/* ---- windows ---- */

/* macdrv_window_features, bit for bit. */
#define WMH_WF_TITLE_BAR                (1u << 0)
#define WMH_WF_CLOSE_BUTTON             (1u << 1)
#define WMH_WF_MINIMIZE_BUTTON          (1u << 2)
#define WMH_WF_RESIZABLE                (1u << 3)
#define WMH_WF_MAXIMIZE_BUTTON          (1u << 4)
#define WMH_WF_UTILITY                  (1u << 5)
#define WMH_WF_SHADOW                   (1u << 6)
#define WMH_WF_PREVENTS_APP_ACTIVATION  (1u << 7)

/* macdrv_window_state, bit for bit.
 *
 * Minimizing, maximizing and fullscreen. The program decides; the host
 * shows what it decided:
 * - The user's minimize (button, Dock, Cmd-M): the host sends
 *   WINDOW_MINIMIZE_REQUESTED and doesn't miniaturize yet. If the program
 *   agrees, window_state comes with MINIMIZED (and MINIMIZED_VALID), and
 *   the host miniaturizes then. A program can minimize itself the same
 *   way, unasked.
 * - window_state without MINIMIZED (MINIMIZED_VALID set) while the window
 *   is miniaturized: the host deminiaturizes it, and sends nothing back.
 * - The user's deminiaturize (Dock, clicking the window's tile): the host
 *   deminiaturizes and sends WINDOW_DID_UNMINIMIZE, then WINDOW_FRAME_CHANGED
 *   with the window's frame, even though it hasn't changed, as the plain
 *   driver does (windowDidDeminiaturize: calls windowDidResize:). Wine
 *   restores the window to a rect worked out from that frame using the
 *   minimized window's rects, which have no title bar or borders, so the
 *   restored window is a little smaller; the frame change corrects it.
 * - The user's zoom (green button or double-click on the title bar, when
 *   the window has WMH_WF_MAXIMIZE_BUTTON): the host sends
 *   WINDOW_MAXIMIZE_REQUESTED and doesn't resize; a frame follows. When
 *   the window is MAXIMIZED, zoom or a live resize sends
 *   WINDOW_RESTORE_REQUESTED instead (flags 1 and the current frame for a
 *   resize: keep that frame).
 * - Fullscreen in its own Space (the host's toggleFullScreen:, offered for
 *   a resizable window that isn't MAXIMIZED or DISABLED): the host sends
 *   WINDOW_FRAME_CHANGED with WMH_FRAME_FULLSCREEN and the screen's
 *   frame on entering, and the frame it had before, without the flag, on
 *   leaving. A program that makes itself fullscreen (a borderless window
 *   covering the screen) isn't put in a Space: it just gets that frame. */
#define WMH_WS_DISABLED                 (1u << 0)
#define WMH_WS_NO_FOREGROUND            (1u << 1)
#define WMH_WS_FLOATING                 (1u << 2)
#define WMH_WS_EXCLUDED_BY_EXPOSE       (1u << 3)
#define WMH_WS_EXCLUDED_BY_CYCLE        (1u << 4)
#define WMH_WS_MINIMIZED                (1u << 5)
#define WMH_WS_MINIMIZED_VALID          (1u << 6)   /* MINIMIZED means something only with this */
#define WMH_WS_MAXIMIZED                (1u << 7)

struct wmh_window_create {
    wmh_window_id   window;
    wmh_window_id   owner;              /* 0: none; same connection only in v1 */
    uint32_t        features;           /* WMH_WF_* */
    uint32_t        state;              /* WMH_WS_* */
    struct wmh_rect frame;              /* the content area */
    uint64_t        hwnd;               /* for logs only */
};

struct wmh_window_ref    { wmh_window_id window; uint32_t reserved; };  /* destroy, hide, focus, focus_rejected */
/* window_focus: make the window the key window. reserved carries
 * WMH_FOCUS_ACTIVATE when the plain driver would also bring the app to the
 * front (activate ignoring other apps); without it, the host makes the
 * window key without activating the app. */
#define WMH_FOCUS_ACTIVATE 0x1u
struct wmh_window_flags  { wmh_window_id window; uint32_t flags; };     /* features, state */
struct wmh_window_frame  { wmh_window_id window; uint32_t reserved; struct wmh_rect frame; };
struct wmh_window_parent { wmh_window_id window; wmh_window_id parent; };

/* The content size the user may resize the window to (WM_GETMINMAXINFO).
 * 0 for a max dimension: no limit. */
struct wmh_window_min_max {
    wmh_window_id window;
    uint32_t      reserved;
    double        min_width, min_height, max_width, max_height;
};
struct wmh_window_title  { wmh_window_id window; uint32_t reserved; char title[WMH_TITLE_MAX]; };

/* macdrv_order_cocoa_window(window, prev, next): below prev if there is
 * one, else above next, else above every window of the connection. */
#define WMH_ORDER_ABOVE 0u
#define WMH_ORDER_BELOW 1u
struct wmh_window_order {
    wmh_window_id window;
    wmh_window_id relative;             /* 0: every window of the connection */
    uint32_t      position;             /* WMH_ORDER_* */
    uint32_t      activate;             /* nonzero: the program wants it active */
};

/* ---- surfaces ---- */

#define WMH_PIXEL_BGRA8 0x42475241u     /* 'BGRA', kCVPixelFormatType_32BGRA, premultiplied */

struct wmh_surface_add {                /* plus the IOSurface port descriptor */
    wmh_surface_id surface;
    uint32_t       width, height;       /* pixels */
    uint32_t       pixel_format;        /* WMH_PIXEL_* */
};

struct wmh_surface_ref { wmh_surface_id surface; uint32_t reserved; };  /* remove, release */

/* The surface belongs to the host from here until it sends surface_release.
 * rect: where the surface goes in the window's content area; dirty: the
 * part that changed, a hint (the host shows the whole surface). Both in
 * the protocol's space, relative to the content area. The surface is
 * drawn at rect's origin at its own size, not scaled into the window, and
 * clipped to the content area. It needn't cover it: under an emulated
 * display mode a fullscreen window's picture is centred (rect at 320,0
 * in a wider window, say) and can reach past the window's edges. What it
 * doesn't cover is black, as with the plain driver. A host drops (and
 * releases) a frame whose width and height don't match the surface as
 * registered. */
struct wmh_window_frame_ready {
    wmh_window_id   window;
    wmh_surface_id  surface;
    uint32_t        width, height;
    struct wmh_rect rect;
    struct wmh_rect dirty;
};

/* ---- views ----
 *
 * A view is a region of a window that a program draws into with OpenGL
 * (Direct3D through wined3d, DirectDraw wrappers, games): a layer over the
 * window's own picture, shown only while not hidden (views start hidden).
 * Its pictures come in view_frame_ready, from surfaces registered and
 * released as for windows. They're bottom-up (row 0 is the bottom) when
 * WMH_SURFACE_BOTTOM_UP is set, as OpenGL draws them; the host flips them.
 * A host scales the surface to the view's frame. */

struct wmh_view_ref { uint32_t view; uint32_t reserved; };

struct wmh_view_frame {
    uint32_t        view;
    wmh_window_id   window;             /* the window it's in; 0: in none, don't show */
    struct wmh_rect frame;              /* in the window's content area, points */
};

struct wmh_view_hidden { uint32_t view; uint32_t hidden; };

#define WMH_SURFACE_BOTTOM_UP (1u << 0)

struct wmh_view_frame_ready {
    uint32_t       view;
    wmh_surface_id surface;
    uint32_t       width, height;       /* as registered */
    uint32_t       flags;               /* WMH_SURFACE_* */
    uint32_t       reserved;
};

/* ---- cursor ----
 *
 * cursor_set: what the pointer looks like over the program's windows. A
 * system cursor is named as NSCursor's class method ("arrowCursor",
 * "IBeamCursor", "pointingHandCursor", ...). An image comes as a BGRA
 * IOSurface (premultiplied, top row first), shown at one point per image
 * pixel, with its hot spot in image pixels; animated cursors send their
 * first frame. The driver sends it when the cursor changes.
 *
 * cursor_warp: the program moved the pointer (SetCursorPos). The host
 * decides whether to move the real one; either way, its next mouse events
 * say where the pointer is.
 *
 * cursor_clip: the program confined the pointer (ClipCursor) to a rect, or
 * released it. The host confines the real pointer, as the plain driver
 * does, so the user can't take it out of the rect (say, to drag a window
 * by its title bar). Keeping only the reported positions inside the rect
 * lets the two part. */

#define WMH_CURSOR_HIDDEN 0u
#define WMH_CURSOR_SYSTEM 1u
#define WMH_CURSOR_IMAGE  2u

struct wmh_cursor_set {
    uint32_t type;                      /* WMH_CURSOR_* */
    uint32_t width, height;             /* IMAGE: pixels */
    uint32_t reserved;
    double   hot_x, hot_y;              /* IMAGE: image pixels from the top left */
    char     name[64];                  /* SYSTEM: NUL-terminated */
};

struct wmh_cursor_warp { double x, y; };

struct wmh_cursor_clip {
    uint32_t        clipped;            /* 0: released */
    uint32_t        reserved;
    struct wmh_rect rect;
};

/* ---- calls and replies ---- */

struct wmh_window_get_frame { wmh_window_id window; uint32_t reserved; };

#define WMH_STATUS_OK          0u
#define WMH_STATUS_UNSUPPORTED 1u       /* the driver uses its fallback answer */
#define WMH_STATUS_FAILED      2u

struct wmh_reply { uint32_t status; uint32_t reserved; };   /* then the reply struct, if OK */

struct wmh_reply_rect  { struct wmh_rect rect; };           /* window_get_frame */
struct wmh_reply_point { double x, y; };                    /* cursor_get */

/* displays_get: the displays the host shows windows on, in the protocol's
 * space. The first is the primary and must be at the origin; the driver
 * gives Wine these as its monitors (frame, work area) and the frame's size
 * as each one's current mode, and uses the real displays if the first
 * isn't at the origin. A real screen with no display here is left out.
 * The pixel sizes aren't used by the driver yet. */
#define WMH_DISPLAYS_MAX 8u
struct wmh_display {
    struct wmh_rect frame, work_area;
    uint32_t        pixels_wide, pixels_high;
};
struct wmh_reply_displays {             /* the first is the primary */
    uint32_t           count;
    uint32_t           reserved;
    struct wmh_display displays[WMH_DISPLAYS_MAX];
};

struct wmh_quit_reply { uint32_t quitting; uint32_t reserved; };

/* ---- text input through an input method ----
 *
 * The host's text input context does the input method's work, driven by the
 * driver, the way the plain driver drives its own:
 * - The host sends key presses as KEY_PRESS events and doesn't give them to
 *   an input context itself.
 * - When a program has an IME open and the current input source is an
 *   input method, the driver calls ime_key with each key press. The host
 *   makes a key-down NSEvent from it (from a CGEvent, which input methods
 *   accept, with the keycode, flags and autorepeat), gives it to the
 *   input context of its key window's text input client
 *   ([inputContext handleEvent:]), and records what the input method
 *   calls back, in order: setMarkedText:selectedRange: as a text with
 *   complete 0 and the selection as cursor_begin..cursor_end (UTF-16
 *   units); insertText: and unmarkText as complete 1 (unmarkText with an
 *   empty text). handled is what handleEvent: returned, and 0 if the
 *   input method called doCommandBySelector:, as with the plain driver.
 * - ime_rect: where the program shows its composition, in the protocol's
 *   space, for firstRectForCharacterRange: (candidate windows go below it).
 *   Like a window's frame, it is in the virtual screen, not relative to a
 *   window: the host maps it as it maps that window's picture before
 *   returning it in Cocoa screen coordinates. For a window placed at its
 *   protocol frame that is only the flip; a scaled window needs the
 *   scaling too.
 * - ime_clear: the program closed its IME: discard the marked text
 *   ([inputContext discardMarkedText]) without sending anything.
 * The host's text input client keeps the marked text, its selection, and
 * the attributed substrings the input method asks for; it doesn't edit
 * anything else. */

#define WMH_IME_TEXTS_MAX 4u
#define WMH_IME_TEXT_MAX  255u              /* UTF-16 units; longer is cut */

struct wmh_ime_key {
    uint32_t keycode;                   /* Mac virtual key code */
    uint32_t modifiers;                 /* CGEventFlags, low 32 bits */
    uint32_t repeat;                    /* nonzero: autorepeat */
    uint32_t reserved;
};

struct wmh_ime_text {
    uint32_t complete;                  /* nonzero: insertText (committed); 0: setMarkedText */
    uint32_t cursor_begin, cursor_end;  /* marked text: the selection, UTF-16 units */
    uint32_t length;                    /* UTF-16 units in text */
    uint16_t text[WMH_IME_TEXT_MAX + 1];
};

struct wmh_reply_ime_key {
    uint32_t           handled;         /* nonzero: the input method took the key */
    wmh_window_id      window;          /* the window whose text input client it was */
    uint32_t           count;           /* texts used */
    uint32_t           reserved;
    struct wmh_ime_text texts[WMH_IME_TEXTS_MAX];
};

struct wmh_ime_rect { struct wmh_rect rect; };

/* ---- drag and drop onto a window ----
 *
 * The host is the drag destination for its windows and passes each step
 * on as a drag message, as the plain driver's NSDraggingDestination
 * methods do. The driver reads the dragged data itself from the drag
 * pasteboard, which it opens by name (pasteboards are system-wide), and
 * answers every drag with drag_done carrying the drag's header serial.
 * The steps go through the window's event queue, in order with its other
 * events, so the host waits for each answer before returning from the
 * AppKit method, as the plain driver does, for as long as it does:
 * - draggingEntered: ENTER (pasteboard set), wait 0.1 s, then as for
 *   draggingUpdated;
 * - draggingUpdated: UPDATE, wait 3 s; the operation to return is
 *   drag_done's ops if accepted, else NSDragOperationNone;
 * - draggingExited: LEAVE, wait 0.1 s;
 * - performDragOperation: DROP, wait up to 180 s; return accepted.
 * A host that times out goes on as if refused. Dragging out of a Wine
 * window isn't supported, as with the plain driver. */

#define WMH_DRAG_ENTER  1u
#define WMH_DRAG_UPDATE 2u
#define WMH_DRAG_LEAVE  3u
#define WMH_DRAG_DROP   4u
#define WMH_PASTEBOARD_NAME_MAX 128u

struct wmh_drag {
    uint32_t      type;                 /* WMH_DRAG_* */
    wmh_window_id window;
    double        x, y;                 /* UPDATE, DROP: in the window's content area, protocol space */
    uint32_t      ops;                  /* UPDATE, DROP: the source's NSDragOperation mask */
    uint32_t      reserved;
    char          pasteboard[WMH_PASTEBOARD_NAME_MAX];  /* ENTER: the drag pasteboard's name, NUL-terminated */
};

struct wmh_drag_done {
    uint32_t serial;                    /* the drag's header serial */
    uint32_t accepted;                  /* nonzero: the program took it */
    uint32_t ops;                       /* UPDATE: the NSDragOperation to show */
    uint32_t reserved;
};

/* ---- events, host -> driver: the macdrv_event types they become ---- */

enum {
    WMH_EV_KEY_PRESS = 1, WMH_EV_KEY_RELEASE, WMH_EV_KEYBOARD_CHANGED,
    WMH_EV_MOUSE_BUTTON = 10, WMH_EV_MOUSE_MOVED_ABSOLUTE, WMH_EV_MOUSE_MOVED_RELATIVE, WMH_EV_MOUSE_SCROLL,
    WMH_EV_WINDOW_GOT_FOCUS = 20, WMH_EV_WINDOW_LOST_FOCUS, WMH_EV_APP_DEACTIVATED,
    WMH_EV_WINDOW_BROUGHT_FORWARD, WMH_EV_RELEASE_CAPTURE,
    WMH_EV_WINDOW_CLOSE_REQUESTED = 30, WMH_EV_WINDOW_FRAME_CHANGED, WMH_EV_WINDOW_RESIZE_ENDED,
    WMH_EV_WINDOW_DID_UNMINIMIZE, WMH_EV_WINDOW_RESTORE_REQUESTED, WMH_EV_REASSERT_WINDOW_POSITION,
    WMH_EV_WINDOW_MINIMIZE_REQUESTED, WMH_EV_WINDOW_MAXIMIZE_REQUESTED,
    WMH_EV_APP_QUIT_REQUESTED = 40, WMH_EV_DISPLAYS_CHANGED,
    /* Questions the driver answers with messages, not a reply:
     * QUERY_MIN_MAX_INFO: send window_min_max (the program's WM_GETMINMAXINFO);
     * QUERY_RESIZE_START: the user is starting to resize or move the window
     * (WM_ENTERSIZEMOVE; WINDOW_RESIZE_ENDED ends it); window_min_max follows. */
    WMH_EV_QUERY_MIN_MAX_INFO = 50, WMH_EV_QUERY_RESIZE_START,
};

/* WINDOW_FRAME_CHANGED flags */
#define WMH_FRAME_FULLSCREEN           (1u << 0)
#define WMH_FRAME_IN_RESIZE            (1u << 1)
#define WMH_FRAME_SKIP_SIZE_MOVE_LOOP  (1u << 2)

/* APP_QUIT_REQUESTED reasons (macdrv's QUIT_REASON_*) */
#define WMH_QUIT_REASON_NONE      0u
#define WMH_QUIT_REASON_LOGOUT    1u
#define WMH_QUIT_REASON_RESTART   2u
#define WMH_QUIT_REASON_SHUTDOWN  3u

/* One struct for every event: the fields its type uses, the rest zero. */
struct wmh_event {
    uint32_t        type;               /* WMH_EV_* */
    wmh_window_id   window;             /* 0 for app events */
    double          time;               /* seconds, the clock of -[NSEvent timestamp] */
    /* keys */
    uint32_t        keycode;            /* Mac virtual key code */
    uint32_t        modifiers;          /* CGEventFlags, low 32 bits */
    /* mouse: the protocol's space, the host's scaling undone */
    double          x, y;
    int32_t         button;             /* 0 left, 1 right, 2 middle, 3.. others */
    uint32_t        pressed;            /* MOUSE_BUTTON: nonzero pressed; MOUSE_MOVED_*: nonzero dragging */
    double          scroll_x, scroll_y; /* in WHEEL_DELTA units, as the plain driver sends */
    /* frame changed, restore, quit */
    struct wmh_rect frame;
    uint32_t        flags;              /* FRAME_CHANGED: WMH_FRAME_*; RESTORE_REQUESTED: 1 keep frame */
    uint32_t        reason;             /* APP_QUIT_REQUESTED: WMH_QUIT_REASON_* */
};

#define WMH_ASSERT_SIZE(t, n) _Static_assert(sizeof(struct t) == (n), #t " size")
WMH_ASSERT_SIZE(wmh_header, 8);
WMH_ASSERT_SIZE(wmh_rect, 32);
WMH_ASSERT_SIZE(wmh_hello, 272);
WMH_ASSERT_SIZE(wmh_hello_reply, 16);
WMH_ASSERT_SIZE(wmh_window_create, 56);
WMH_ASSERT_SIZE(wmh_window_title, 520);
WMH_ASSERT_SIZE(wmh_window_order, 16);
WMH_ASSERT_SIZE(wmh_window_frame, 40);
WMH_ASSERT_SIZE(wmh_window_min_max, 40);
WMH_ASSERT_SIZE(wmh_surface_add, 16);
WMH_ASSERT_SIZE(wmh_window_frame_ready, 80);
WMH_ASSERT_SIZE(wmh_view_frame, 40);
WMH_ASSERT_SIZE(wmh_view_frame_ready, 24);
WMH_ASSERT_SIZE(wmh_cursor_set, 96);
WMH_ASSERT_SIZE(wmh_cursor_clip, 40);
WMH_ASSERT_SIZE(wmh_reply, 8);
WMH_ASSERT_SIZE(wmh_ime_key, 16);
WMH_ASSERT_SIZE(wmh_ime_text, 528);
WMH_ASSERT_SIZE(wmh_reply_ime_key, 16 + 4 * 528);
WMH_ASSERT_SIZE(wmh_ime_rect, 32);
WMH_ASSERT_SIZE(wmh_reply_displays, 8 + 8 * 72);
WMH_ASSERT_SIZE(wmh_event, 104);
WMH_ASSERT_SIZE(wmh_drag, 160);
WMH_ASSERT_SIZE(wmh_drag_done, 16);
#undef WMH_ASSERT_SIZE

#endif /* WMH_PROTOCOL_H */
