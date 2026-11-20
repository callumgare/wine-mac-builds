/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * App-embedded Mac driver: host protocol wire format, version 1 (draft).
 * The design is docs/app-embedded-protocol.md; this file fixes the bytes.
 * The driver (winemac.drv, x86_64 under Rosetta 2) and hosts (any
 * architecture) both use it. Swift can import it through a bridging header
 * or module map.
 *
 * Every message is one Mach message:
 *
 *   mach_msg_header_t      msgh_id = the message kind (WMH_*)
 *   [mach_msg_body_t + descriptors]   only for the kinds marked "port" below
 *   struct wmh_header      the same kind again, and the serial
 *   the kind's payload     struct wmh_<kind>, then any trailing bytes
 *
 * A message with descriptors sets MACH_MSGH_BITS_COMPLEX. Payloads use
 * fixed-width types and explicit padding only, so they're the same on
 * x86_64 and arm64; the static asserts at the end check the sizes.
 * Variable-length text (UTF-8, not NUL-terminated) follows its struct, and
 * the struct says how long it is; the message is padded to 4 bytes.
 *
 * Ids (window, view, surface) are chosen by the driver, per connection,
 * and never 0. 0 means "none".
 *
 * Positions and sizes are in the protocol's space (see the design doc):
 * points, origin at the top left of the primary display, y down. Surface
 * sizes are in pixels.
 */

#ifndef WMH_PROTOCOL_H
#define WMH_PROTOCOL_H

#include <stdint.h>

#define WMH_PROTOCOL_VERSION 1

/* Longest text that follows a struct (a title, an exe name). */
#define WMH_MAX_TEXT 4096

/* Message kinds. Driver -> host are 0x1xx, host -> driver 0x2xx. Kinds
 * not listed here are reserved for the rest of version 1 (cursor, views,
 * displays, queries); a side that gets a kind it doesn't know ignores it,
 * and answers a call it doesn't know with WMH_REPLY_UNSUPPORTED. */
enum wmh_kind {
    /* driver -> host */
    WMH_HELLO               = 0x100,  /* port: the driver's receive port (send right) */
    WMH_SURFACE_ADD         = 0x101,  /* port: the IOSurface (IOSurfaceCreateMachPort, moved) */
    WMH_SURFACE_REMOVE      = 0x102,
    WMH_WINDOW_CREATE       = 0x110,
    WMH_WINDOW_DESTROY      = 0x111,
    WMH_WINDOW_FEATURES     = 0x112,
    WMH_WINDOW_STATE        = 0x113,
    WMH_WINDOW_TITLE        = 0x114,
    WMH_WINDOW_ORDER        = 0x115,
    WMH_WINDOW_HIDE         = 0x116,
    WMH_WINDOW_FRAME        = 0x117,
    WMH_WINDOW_PARENT       = 0x118,
    WMH_WINDOW_FRAME_READY  = 0x119,
    WMH_WINDOW_FOCUS        = 0x11a,
    WMH_WINDOW_GET_FRAME    = 0x11b,  /* call: answered with WMH_REPLY_FRAME */
    WMH_FOCUS_REJECTED      = 0x11c,
    WMH_QUIT_REPLY          = 0x140,

    /* host -> driver */
    WMH_HELLO_REPLY         = 0x200,
    WMH_SURFACE_RELEASE     = 0x201,
    WMH_REPLY_FRAME         = 0x202,  /* answers WMH_WINDOW_GET_FRAME */
    WMH_REPLY_UNSUPPORTED   = 0x203,  /* answers any call the host doesn't know */
    WMH_KEY_PRESS           = 0x210,
    WMH_KEY_RELEASE         = 0x211,
    WMH_MOUSE_BUTTON        = 0x212,
    WMH_MOUSE_MOVED_ABSOLUTE= 0x213,
    WMH_MOUSE_SCROLL        = 0x214,
    WMH_WINDOW_GOT_FOCUS    = 0x220,
    WMH_WINDOW_LOST_FOCUS   = 0x221,
    WMH_APP_DEACTIVATED     = 0x222,
    WMH_WINDOW_CLOSE_REQUESTED = 0x223,
    WMH_WINDOW_FRAME_CHANGED   = 0x224,
    WMH_APP_QUIT_REQUESTED     = 0x230,
};

struct wmh_header {
    uint32_t kind;      /* the same as msgh_id */
    uint32_t serial;    /* nonzero on a call that wants a reply; the reply carries it back */
};

struct wmh_rect {
    double x, y, width, height;
};

/* struct macdrv_window_features, as bits */
enum {
    WMH_FEATURE_TITLE_BAR           = 1 << 0,
    WMH_FEATURE_CLOSE_BUTTON        = 1 << 1,
    WMH_FEATURE_MINIMIZE_BUTTON     = 1 << 2,
    WMH_FEATURE_RESIZABLE           = 1 << 3,
    WMH_FEATURE_MAXIMIZE_BUTTON     = 1 << 4,
    WMH_FEATURE_UTILITY             = 1 << 5,
    WMH_FEATURE_SHADOW              = 1 << 6,
    WMH_FEATURE_PREVENTS_ACTIVATION = 1 << 7,
};

/* struct macdrv_window_state, as bits */
enum {
    WMH_STATE_DISABLED          = 1 << 0,
    WMH_STATE_NO_FOREGROUND     = 1 << 1,
    WMH_STATE_FLOATING          = 1 << 2,
    WMH_STATE_MINIMIZED         = 1 << 5,
    WMH_STATE_MINIMIZED_VALID   = 1 << 6,  /* MINIMIZED means something only with this */
    WMH_STATE_MAXIMIZED         = 1 << 7,
};

/* Handshake */

struct wmh_hello {
    uint32_t version;       /* WMH_PROTOCOL_VERSION */
    int32_t  pid;
    uint32_t exe_length;    /* bytes of UTF-8 that follow: the exe's file name, e.g. "ski32.exe" */
    uint32_t pad;
};

/* hello_reply features */
enum {
    WMH_HOST_GDI      = 1 << 0,   /* takes window surfaces (required in v1) */
    WMH_HOST_OPENGL   = 1 << 1,   /* takes view surfaces */
    WMH_HOST_CURSOR   = 1 << 2,
};

struct wmh_hello_reply {
    uint32_t version;       /* WMH_PROTOCOL_VERSION, or 0 to refuse the process */
    uint32_t features;      /* WMH_HOST_* */
};

/* Surfaces */

#define WMH_PIXEL_FORMAT_BGRA 0x42475241u  /* 'BGRA', 32 bits per pixel, premultiplied */

struct wmh_surface_add {
    uint32_t surface;
    uint32_t width;         /* pixels */
    uint32_t height;
    uint32_t pixel_format;  /* WMH_PIXEL_FORMAT_* */
};

struct wmh_surface_remove  { uint32_t surface; uint32_t pad; };
struct wmh_surface_release { uint32_t surface; uint32_t pad; };

/* Windows */

struct wmh_window_create {
    uint32_t window;
    uint32_t owner;         /* 0, or the window that owns it (same process) */
    uint64_t hwnd;          /* for logs only */
    struct wmh_rect frame;
    uint32_t features;      /* WMH_FEATURE_* */
    uint32_t pad;
};

struct wmh_window          { uint32_t window; uint32_t pad; };  /* destroy, hide, focus, focus_rejected */
struct wmh_window_features { uint32_t window; uint32_t features; };
struct wmh_window_state    { uint32_t window; uint32_t state; };

struct wmh_window_title {
    uint32_t window;
    uint32_t length;        /* bytes of UTF-8 that follow */
};

/* Place window directly below `below`, or else directly above `above`
 * (macdrv_order_cocoa_window's prev and next); both 0: on top. */
struct wmh_window_order {
    uint32_t window;
    uint32_t below;
    uint32_t above;
    uint32_t activate;      /* 0 or 1 */
};

struct wmh_window_frame {
    uint32_t window;
    uint32_t pad;
    struct wmh_rect frame;
};

struct wmh_window_parent   { uint32_t window; uint32_t parent; };

/* The surface now belongs to the host until it sends surface_release.
 * rect: where the surface's picture goes, in the window's content area
 * (points); dirty: the part that changed (a hint). Drop the frame if
 * width/height don't match the surface as registered. */
struct wmh_window_frame_ready {
    uint32_t window;
    uint32_t surface;
    uint32_t width;
    uint32_t height;
    struct wmh_rect rect;
    struct wmh_rect dirty;
};

struct wmh_reply_frame {
    struct wmh_rect frame;
};

struct wmh_quit_reply { uint32_t quit; uint32_t pad; };  /* 1: the program agreed to quit */

/* Input (host -> driver). Positions in the protocol's space, in points,
 * rounded to integers as the plain driver does. time_ms: milliseconds
 * since boot (as -[NSEvent timestamp] * 1000). */

struct wmh_key {
    uint32_t window;
    uint16_t keycode;       /* Mac virtual key code */
    uint16_t pad;
    uint64_t modifiers;     /* CGEventFlags */
    uint64_t time_ms;
};

struct wmh_mouse_button {
    uint32_t window;
    int32_t  button;        /* 0 left, 1 right, 2 middle, 3.. others */
    int32_t  pressed;       /* 0 or 1 */
    int32_t  x, y;
    uint32_t pad;
    uint64_t time_ms;
};

struct wmh_mouse_moved {
    uint32_t window;
    int32_t  x, y;
    int32_t  drag;          /* 0 or 1: a button is down */
    uint64_t time_ms;
};

struct wmh_mouse_scroll {
    uint32_t window;
    int32_t  x_scroll, y_scroll;   /* in WHEEL_DELTA units, as the plain driver sends */
    int32_t  x, y;
    uint32_t pad;
    uint64_t time_ms;
};

/* got_focus, lost_focus, close_requested: struct wmh_window */

struct wmh_window_frame_changed {
    uint32_t window;
    uint32_t flags;         /* WMH_FRAME_* */
    struct wmh_rect frame;
};

enum {
    WMH_FRAME_FULLSCREEN          = 1 << 0,
    WMH_FRAME_IN_RESIZE           = 1 << 1,
    WMH_FRAME_SKIP_SIZE_MOVE_LOOP = 1 << 2,
};

enum {
    WMH_QUIT_REASON_NONE,
    WMH_QUIT_REASON_LOGOUT,
    WMH_QUIT_REASON_RESTART,
    WMH_QUIT_REASON_SHUTDOWN,
};

struct wmh_app_quit_requested { uint32_t reason; uint32_t pad; };

#define WMH_ASSERT_SIZE(t, n) _Static_assert(sizeof(struct t) == (n), #t " size")
WMH_ASSERT_SIZE(wmh_header, 8);
WMH_ASSERT_SIZE(wmh_rect, 32);
WMH_ASSERT_SIZE(wmh_hello, 16);
WMH_ASSERT_SIZE(wmh_hello_reply, 8);
WMH_ASSERT_SIZE(wmh_surface_add, 16);
WMH_ASSERT_SIZE(wmh_window_create, 56);
WMH_ASSERT_SIZE(wmh_window_order, 16);
WMH_ASSERT_SIZE(wmh_window_frame, 40);
WMH_ASSERT_SIZE(wmh_window_frame_ready, 80);
WMH_ASSERT_SIZE(wmh_key, 24);
WMH_ASSERT_SIZE(wmh_mouse_button, 32);
WMH_ASSERT_SIZE(wmh_mouse_moved, 24);
WMH_ASSERT_SIZE(wmh_mouse_scroll, 32);
WMH_ASSERT_SIZE(wmh_window_frame_changed, 40);
#undef WMH_ASSERT_SIZE

#endif /* WMH_PROTOCOL_H */
