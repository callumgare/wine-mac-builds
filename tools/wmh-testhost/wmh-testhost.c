/* A headless test host for winemac.drv's host mode: logs the driver's
 * messages, releases every frame, saves the last picture of each window as
 * a PNG, and after a while asks the visible windows to close.
 *
 *   wmh-testhost <service> <seconds-before-close> <out-dir> [command args...]
 *
 * It checks <service> in, so run it as a LaunchAgent that declares the
 * service under MachServices, as the job's own process (exec it from the
 * job's script); it then starts [command], which runs Wine with
 * WINEMAC_HOST_SERVICE=<service> and HostWindows set. Host mode puts
 * nothing on screen, so this works while someone uses the Mac.
 *
 * Build (native): clang -O1 -o wmh-testhost wmh-testhost.c -framework IOSurface
 *   -framework CoreGraphics -framework ImageIO -framework CoreFoundation
 *   -framework ApplicationServices
 */
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <IOSurface/IOSurfaceRef.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../docs/app-embedded-protocol.h"
#include <spawn.h>
extern char **environ;

#define MAXW 1024
struct conn {
    mach_port_t recv, driver;
    int pid;
    char exe[64];
    IOSurfaceRef surfaces[MAXW];
    unsigned frames[MAXW], visible[MAXW], view_frames[MAXW], view_window[MAXW], moved[MAXW];
    struct wmh_rect wframe[MAXW];
    char titles[MAXW][64];
};
static struct conn conns[32];
static int nconns;
static const char *outdir;

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static uint32_t reply_serial;   /* send_to: answer this call instead of numbering a new message */

static uint32_t send_to(mach_port_t port, uint32_t kind, const void *payload, size_t size, mach_port_t give, int give_disp)
{
    static uint32_t next_serial = 1;
    uint32_t serial = reply_serial ? reply_serial : next_serial++;
    struct { mach_msg_header_t h; mach_msg_body_t b; mach_msg_port_descriptor_t d; struct wmh_header w; char p[4096]; } m;
    struct { mach_msg_header_t h; struct wmh_header w; char p[4096]; } *pm = (void *)&m;
    kern_return_t kr;

    memset(&m, 0, sizeof(m));
    if (give)
    {
        m.h.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
        m.b.msgh_descriptor_count = 1;
        m.d.name = give; m.d.disposition = give_disp; m.d.type = MACH_MSG_PORT_DESCRIPTOR;
        m.w.kind = kind; m.w.serial = serial; memcpy(m.p, payload, size);
        m.h.msgh_size = offsetof(typeof(m), p) + ((size + 3) & ~3);
    }
    else
    {
        pm->h.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
        pm->w.kind = kind; pm->w.serial = serial; memcpy(pm->p, payload, size);
        pm->h.msgh_size = sizeof(pm->h) + sizeof(pm->w) + ((size + 3) & ~3);
    }
    m.h.msgh_remote_port = port;
    m.h.msgh_id = WMH_MSG_ID;
    kr = mach_msg(&m.h, MACH_SEND_MSG | MACH_SEND_TIMEOUT, m.h.msgh_size, 0, 0, 1000, 0);
    if (kr) printf("host: send %u failed %x\n", kind, kr);
    return serial;
}

/* WMH_TESTHOST_DROP=<path>: put the file on a pasteboard of our own, the
 * way a drag would, and return its name. */
static const char *drop_pasteboard(const char *path)
{
    static const char *name = "org.wine-mac-builds.testhost.drop";
    PasteboardRef pb;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), false);
    CFDataRef data = CFURLCreateData(NULL, url, kCFStringEncodingUTF8, true);
    CFStringRef cfname = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);

    if (PasteboardCreate(cfname, &pb) || PasteboardClear(pb) ||
        PasteboardPutItemFlavor(pb, (PasteboardItemID)1, CFSTR("public.file-url"), data, 0))
        printf("host: can't put %s on a pasteboard\n", path);
    PasteboardSynchronize(pb);
    CFRelease(cfname); CFRelease(data); CFRelease(url);
    return name;
}

static void save_png(struct conn *c, const char *what, uint32_t wid, IOSurfaceRef s, int bottom_up)
{
    char path[1024];
    size_t w = IOSurfaceGetWidth(s), h = IOSurfaceGetHeight(s);
    IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(IOSurfaceGetBaseAddress(s), w, h, 8, IOSurfaceGetBytesPerRow(s), cs,
                                             kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGImageRef img = ctx ? CGBitmapContextCreateImage(ctx) : NULL;
    IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL);
    if (img && bottom_up)
    {
        CGContextRef flip = CGBitmapContextCreate(NULL, w, h, 8, 0, cs, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        CGContextTranslateCTM(flip, 0, h);
        CGContextScaleCTM(flip, 1, -1);
        CGContextDrawImage(flip, CGRectMake(0, 0, w, h), img);
        CGImageRelease(img);
        img = CGBitmapContextCreateImage(flip);
        CGContextRelease(flip);
    }
    snprintf(path, sizeof(path), "%s/%s-%s%u.png", outdir, c->exe, what, wid);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), false);
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, NULL);
    if (dst && img) { CGImageDestinationAddImage(dst, img, NULL); CGImageDestinationFinalize(dst); }
    if (dst) CFRelease(dst);
    CFRelease(url);
    if (img) CGImageRelease(img);
    if (ctx) CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
}

int main(int argc, char **argv)
{
    mach_port_t service, set;
    double close_after = argc > 2 ? atof(argv[2]) : 5, start = 0, deadline = now() + 120;
    int closed = 0, step = 0;
    /* WMH_TESTHOST_STATES=1: before closing, ask the last connection's first
     * window to minimize, report it unminimized, then ask it to maximize. */
    int states = getenv("WMH_TESTHOST_STATES") ? atoi(getenv("WMH_TESTHOST_STATES")) : 0;  /* 2: no maximize */
    /* WMH_TESTHOST_DROP=<path>: drag that file onto the last connection's
     * first window (enter, update at its centre, drop). */
    const char *drop = getenv("WMH_TESTHOST_DROP");
    /* WMH_TESTHOST_IME=1: type "a" then Return into the last connection's
     * first window, and act as an input method when the driver asks:
     * "a" gives the marked text U+3042, Return commits U+4E9C. */
    int ime = getenv("WMH_TESTHOST_IME") != NULL;
    struct { mach_msg_header_t h; char d[8192]; } m;

    outdir = argc > 3 ? argv[3] : ".";
    if (bootstrap_check_in(bootstrap_port, argv[1], &service))
    {
        printf("host: check-in of %s failed\n", argv[1]);
        return 2;
    }
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &set);
    mach_port_insert_member(mach_task_self(), service, set);
    printf("host: listening as %s\n", argv[1]);
    fflush(stdout);
    if (argc > 4)
    {
        pid_t pid;
        if (posix_spawn(&pid, argv[4], NULL, NULL, argv + 4, environ)) printf("host: can't start %s\n", argv[4]);
        else printf("host: started %s (pid %d)\n", argv[4], pid);
        fflush(stdout);
    }

    while (now() < deadline)
    {
        kern_return_t kr = mach_msg(&m.h, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(m), set, 200, 0);
        if (start && !closed && now() - start > close_after)
        {
            for (int ci = 0; ci < nconns; ci++)
                for (uint32_t i = 1; i < MAXW; i++)
                    if (conns[ci].visible[i] && conns[ci].frames[i])
                    {
                        struct wmh_event ev = { .type = WMH_EV_WINDOW_CLOSE_REQUESTED, .window = i };
                        printf("host: asking %s window %u (%s) to close\n", conns[ci].exe, i, conns[ci].titles[i]);
                        send_to(conns[ci].driver, WMH_KIND_EVENT, &ev, sizeof(ev), 0, 0);
                    }
            closed = 1;
            deadline = now() + 15;
        }
        if (states && start && !closed && step < (states == 2 ? 2 : 3) && now() - start > 1.5 * (step + 1) && nconns)
        {
            static const uint32_t types[] = { WMH_EV_WINDOW_MINIMIZE_REQUESTED, WMH_EV_WINDOW_DID_UNMINIMIZE,
                                              WMH_EV_WINDOW_MAXIMIZE_REQUESTED };
            static const char *names[] = { "minimize_requested", "did_unminimize", "maximize_requested" };
            struct conn *sc = &conns[nconns - 1];
            for (uint32_t i = 1; i < MAXW; i++)
                if (sc->visible[i] && sc->frames[i])
                {
                    struct wmh_event ev = { .type = types[step], .window = i };
                    printf("host: sending %s to %s window %u\n", names[step], sc->exe, i);
                    send_to(sc->driver, WMH_KIND_EVENT, &ev, sizeof(ev), 0, 0);
                    if (types[step] == WMH_EV_WINDOW_DID_UNMINIMIZE)
                    {
                        /* As the plain driver's windowDidDeminiaturize: does (through
                           windowDidResize:), say where the window is now. */
                        struct wmh_event fc = { .type = WMH_EV_WINDOW_FRAME_CHANGED, .window = i, .frame = sc->wframe[i] };
                        printf("host: sending frame_changed %.0f,%.0f %.0fx%.0f\n", fc.frame.x, fc.frame.y,
                               fc.frame.width, fc.frame.height);
                        send_to(sc->driver, WMH_KIND_EVENT, &fc, sizeof(fc), 0, 0);
                    }
                    break;
                }
            step++;
            fflush(stdout);
        }
        if (drop && start && !closed && step < 3 && now() - start > 1.5 * (step + 1) && nconns)
        {
            struct conn *sc = &conns[nconns - 1];
            for (uint32_t i = 1; i < MAXW; i++)
                if (sc->visible[i] && sc->frames[i])
                {
                    struct wmh_drag d = { .window = i, .x = sc->wframe[i].width / 2, .y = sc->wframe[i].height / 2,
                                          .ops = 1 /* NSDragOperationCopy */ };
                    uint32_t serial;
                    d.type = step == 0 ? WMH_DRAG_ENTER : step == 1 ? WMH_DRAG_UPDATE : WMH_DRAG_DROP;
                    if (d.type == WMH_DRAG_ENTER) snprintf(d.pasteboard, sizeof(d.pasteboard), "%s", drop_pasteboard(drop));
                    serial = send_to(sc->driver, WMH_KIND_DRAG, &d, sizeof(d), 0, 0);
                    printf("host: drag %u (serial %u) to %s window %u at %.0f,%.0f\n", d.type, serial, sc->exe, i, d.x, d.y);
                    break;
                }
            step++;
            fflush(stdout);
        }
        if (ime && start && !closed && step < 2 && now() - start > 1.5 * (step + 1) && nconns)
        {
            struct conn *sc = &conns[nconns - 1];
            for (uint32_t i = 1; i < MAXW; i++)
                if (sc->visible[i] && sc->frames[i])
                {
                    struct wmh_event k = { .type = WMH_EV_KEY_PRESS, .window = i, .keycode = step == 0 ? 0 : 36 };
                    printf("host: key %u to %s window %u\n", k.keycode, sc->exe, i);
                    send_to(sc->driver, WMH_KIND_EVENT, &k, sizeof(k), 0, 0);
                    k.type = WMH_EV_KEY_RELEASE;
                    send_to(sc->driver, WMH_KIND_EVENT, &k, sizeof(k), 0, 0);
                    break;
                }
            step++;
            fflush(stdout);
        }
        if (kr == MACH_RCV_TIMED_OUT) continue;
        if (kr) { printf("host: receive failed %x\n", kr); break; }
        if (m.h.msgh_id != WMH_MSG_ID) { printf("host: foreign message id %d\n", m.h.msgh_id); continue; }

        char *p = (char *)&m.h + sizeof(m.h);
        mach_port_t port = MACH_PORT_NULL;
        if (m.h.msgh_bits & MACH_MSGH_BITS_COMPLEX)
        {
            mach_msg_body_t *body = (mach_msg_body_t *)p;
            mach_msg_port_descriptor_t *d = (mach_msg_port_descriptor_t *)(body + 1);
            port = d->name;
            p = (char *)(d + body->msgh_descriptor_count);
        }
        struct wmh_header *w = (struct wmh_header *)p;
        void *pl = w + 1;
        struct conn *c = NULL;

        if (w->kind == WMH_KIND_HELLO && m.h.msgh_local_port == service && nconns < 32)
        {
            struct wmh_hello *h = pl;
            struct wmh_hello_reply r = { WMH_PROTOCOL_VERSION, WMH_FEATURE_GDI_SURFACES | WMH_FEATURE_INPUT | WMH_FEATURE_GL_VIEWS | WMH_FEATURE_CURSOR };
            c = &conns[nconns++];
            c->driver = port;
            c->pid = h->pid;
            snprintf(c->exe, sizeof(c->exe), "%s", h->exe_name);
            mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &c->recv);
            mach_port_insert_member(mach_task_self(), c->recv, set);
            printf("host: hello v%u pid %d exe %s features %#x\n", h->protocol_version, h->pid, h->exe_name, h->features);
            send_to(c->driver, WMH_KIND_HELLO_REPLY, &r, sizeof(r), c->recv, MACH_MSG_TYPE_MAKE_SEND);
            fflush(stdout);
            continue;
        }
        for (int ci = 0; ci < nconns; ci++) if (conns[ci].recv == m.h.msgh_local_port) c = &conns[ci];
        if (!c) { printf("host: kind %u on an unknown port\n", w->kind); continue; }

        switch (w->kind)
        {
        case WMH_KIND_WINDOW_CREATE: {
            struct wmh_window_create *cr = pl;
            if (cr->window < MAXW) c->wframe[cr->window] = cr->frame;
            printf("host: %s window_create %u owner %u features %#x frame %.0f,%.0f %.0fx%.0f\n", c->exe, cr->window,
                   cr->owner, cr->features, cr->frame.x, cr->frame.y, cr->frame.width, cr->frame.height);
            break; }
        case WMH_KIND_WINDOW_TITLE: {
            struct wmh_window_title *t = pl;
            if (t->window < MAXW) snprintf(c->titles[t->window], sizeof(c->titles[0]), "%s", t->title);
            printf("host: %s window_title %u \"%s\"\n", c->exe, t->window, t->title);
            break; }
        case WMH_KIND_WINDOW_ORDER: {
            struct wmh_window_order *o = pl;
            printf("host: %s window_order %u %s %u activate %u\n", c->exe, o->window, o->position ? "below" : "above", o->relative, o->activate);
            if (o->window < MAXW) c->visible[o->window] = 1;
            if (!start) start = now();
            break; }
        case WMH_KIND_WINDOW_HIDE:
        case WMH_KIND_WINDOW_DESTROY:
        case WMH_KIND_WINDOW_FOCUS: {
            struct wmh_window_ref *r = pl;
            printf("host: %s %s %u\n", c->exe, w->kind == WMH_KIND_WINDOW_HIDE ? "window_hide" :
                   w->kind == WMH_KIND_WINDOW_DESTROY ? "window_destroy" : "window_focus", r->window);
            if (w->kind != WMH_KIND_WINDOW_FOCUS && r->window < MAXW) c->visible[r->window] = 0;
            if (w->kind == WMH_KIND_WINDOW_FOCUS)
            {
                struct wmh_event ev = { .type = WMH_EV_WINDOW_GOT_FOCUS, .window = r->window };
                send_to(c->driver, WMH_KIND_EVENT, &ev, sizeof(ev), 0, 0);
            }
            break; }
        case WMH_KIND_WINDOW_FRAME: {
            struct wmh_window_frame *f = pl;
            if (f->window < MAXW) c->wframe[f->window] = f->frame;
            printf("host: %s window_frame %u %.0f,%.0f %.0fx%.0f\n", c->exe, f->window, f->frame.x, f->frame.y, f->frame.width, f->frame.height);
            break; }
        case WMH_KIND_SURFACE_ADD: {
            struct wmh_surface_add *a = pl;
            if (a->surface < MAXW) c->surfaces[a->surface] = IOSurfaceLookupFromMachPort(port);
            mach_port_deallocate(mach_task_self(), port);
            printf("host: %s surface_add %u %ux%u %s\n", c->exe, a->surface, a->width, a->height,
                   a->surface < MAXW && c->surfaces[a->surface] ? "mapped" : "NOT MAPPED");
            break; }
        case WMH_KIND_SURFACE_REMOVE: {
            struct wmh_surface_ref *r = pl;
            printf("host: %s surface_remove %u\n", c->exe, r->surface);
            if (r->surface < MAXW && c->surfaces[r->surface]) { CFRelease(c->surfaces[r->surface]); c->surfaces[r->surface] = NULL; }
            break; }
        case WMH_KIND_WINDOW_FRAME_READY: {
            struct wmh_window_frame_ready *f = pl;
            struct wmh_surface_ref r = { f->surface };
            if (f->window < MAXW && c->frames[f->window]++ < 3)
                printf("host: %s frame_ready window %u surface %u %ux%u rect %.0f,%.0f %.0fx%.0f\n", c->exe, f->window, f->surface,
                       f->width, f->height, f->rect.x, f->rect.y, f->rect.width, f->rect.height);
            if (f->surface < MAXW && c->surfaces[f->surface]) save_png(c, "window", f->window, c->surfaces[f->surface], 0);
            send_to(c->driver, WMH_KIND_SURFACE_RELEASE, &r, sizeof(r), 0, 0);
            break; }
        case WMH_KIND_VIEW_CREATE:
        case WMH_KIND_VIEW_DESTROY: {
            struct wmh_view_ref *v = pl;
            printf("host: %s %s %u\n", c->exe, w->kind == WMH_KIND_VIEW_CREATE ? "view_create" : "view_destroy", v->view);
            break; }
        case WMH_KIND_VIEW_FRAME: {
            struct wmh_view_frame *v = pl;
            if (v->view < MAXW) c->view_window[v->view] = v->window;
            printf("host: %s view_frame %u in window %u %.0f,%.0f %.0fx%.0f\n", c->exe, v->view, v->window,
                   v->frame.x, v->frame.y, v->frame.width, v->frame.height);
            break; }
        case WMH_KIND_VIEW_HIDDEN: {
            struct wmh_view_hidden *v = pl;
            printf("host: %s view_hidden %u %u\n", c->exe, v->view, v->hidden);
            break; }
        case WMH_KIND_VIEW_FRAME_READY: {
            struct wmh_view_frame_ready *f = pl;
            struct wmh_surface_ref r = { f->surface };
            if (f->view < MAXW && c->view_window[f->view] < MAXW) c->frames[c->view_window[f->view]]++;
            if (f->view < MAXW && c->view_frames[f->view]++ < 3)
                printf("host: %s view_frame_ready view %u surface %u %ux%u flags %#x\n", c->exe, f->view, f->surface,
                       f->width, f->height, f->flags);
            if (f->surface < MAXW && c->surfaces[f->surface] && f->view < MAXW && c->view_frames[f->view] % 10 == 1)
                save_png(c, "view", f->view, c->surfaces[f->surface], f->flags & WMH_SURFACE_BOTTOM_UP);
            send_to(c->driver, WMH_KIND_SURFACE_RELEASE, &r, sizeof(r), 0, 0);
            break; }
        case WMH_KIND_CURSOR_SET: {
            struct wmh_cursor_set *cs = pl;
            if (cs->type == WMH_CURSOR_SYSTEM) printf("host: %s cursor_set system %s\n", c->exe, cs->name);
            else if (cs->type == WMH_CURSOR_IMAGE)
            {
                IOSurfaceRef img = port ? IOSurfaceLookupFromMachPort(port) : NULL;
                printf("host: %s cursor_set image %ux%u hot %.0f,%.0f %s\n", c->exe, cs->width, cs->height,
                       cs->hot_x, cs->hot_y, img ? "mapped" : "NOT MAPPED");
                if (img) { save_png(c, "cursor", 0, img, 0); CFRelease(img); }
                if (port) mach_port_deallocate(mach_task_self(), port);
            }
            else printf("host: %s cursor_set hidden\n", c->exe);
            break; }
        case WMH_KIND_IME_KEY: {
            struct wmh_ime_key *k = pl;
            struct { struct wmh_reply r; struct wmh_reply_ime_key k; } rep;
            memset(&rep, 0, sizeof(rep));
            rep.r.status = WMH_STATUS_OK;
            for (uint32_t i = 1; i < MAXW; i++) if (c->visible[i] && c->frames[i]) { rep.k.window = i; break; }
            if (k->keycode == 0)
            {
                rep.k.handled = 1; rep.k.count = 1;
                rep.k.texts[0] = (struct wmh_ime_text){ .complete = 0, .cursor_begin = 1, .cursor_end = 1, .length = 1 };
                rep.k.texts[0].text[0] = 0x3042;
            }
            else if (k->keycode == 36)
            {
                rep.k.handled = 1; rep.k.count = 1;
                rep.k.texts[0] = (struct wmh_ime_text){ .complete = 1, .length = 1 };
                rep.k.texts[0].text[0] = 0x4e9c;
            }
            printf("host: %s ime_key %u flags %#x repeat %u (serial %u) -> handled %u\n", c->exe, k->keycode, k->modifiers,
                   k->repeat, w->serial, rep.k.handled);
            reply_serial = w->serial;
            send_to(c->driver, WMH_KIND_REPLY, &rep, sizeof(rep), 0, 0);
            reply_serial = 0;
            break; }
        case WMH_KIND_IME_CLEAR:
            printf("host: %s ime_clear\n", c->exe);
            break;
        case WMH_KIND_IME_RECT: {
            struct wmh_ime_rect *r = pl;
            printf("host: %s ime_rect %.0f,%.0f %.0fx%.0f\n", c->exe, r->rect.x, r->rect.y, r->rect.width, r->rect.height);
            break; }
        case WMH_KIND_DRAG_DONE: {
            struct wmh_drag_done *dd = pl;
            printf("host: %s drag_done serial %u accepted %u ops %#x\n", c->exe, dd->serial, dd->accepted, dd->ops);
            break; }
        case WMH_KIND_WINDOW_STATE: {
            struct wmh_window_flags *st = pl;
            printf("host: %s window_state %u %#x%s%s\n", c->exe, st->window, st->flags,
                   (st->flags & WMH_WS_MINIMIZED) ? " minimized" : "", (st->flags & WMH_WS_MAXIMIZED) ? " maximized" : "");
            break; }
        case WMH_KIND_WINDOW_MIN_MAX: {
            struct wmh_window_min_max *mm = pl;
            printf("host: %s window_min_max %u min %.0fx%.0f max %.0fx%.0f\n", c->exe, mm->window,
                   mm->min_width, mm->min_height, mm->max_width, mm->max_height);
            break; }
        case WMH_KIND_CURSOR_WARP: {
            struct wmh_cursor_warp *cw = pl;
            printf("host: %s cursor_warp %.0f,%.0f\n", c->exe, cw->x, cw->y);
            break; }
        case WMH_KIND_CURSOR_CLIP: {
            struct wmh_cursor_clip *cc = pl;
            printf("host: %s cursor_clip %u %.0f,%.0f %.0fx%.0f\n", c->exe, cc->clipped, cc->rect.x, cc->rect.y, cc->rect.width, cc->rect.height);
            break; }
        default:
            printf("host: %s kind %u\n", c->exe, w->kind);
            break;
        }
        /* After a window's first frame, move the pointer to its centre, so the
           program sets its cursor. */
        for (uint32_t i = 1; i < MAXW; i++)
            if (c->frames[i] && !c->moved[i] && c->visible[i])
            {
                struct wmh_event ev = { .type = WMH_EV_MOUSE_MOVED_ABSOLUTE, .window = i,
                                        .x = c->wframe[i].x + c->wframe[i].width / 2,
                                        .y = c->wframe[i].y + c->wframe[i].height / 2 };
                c->moved[i] = 1;
                printf("host: %s moving the pointer to %.0f,%.0f in window %u\n", c->exe, ev.x, ev.y, i);
                send_to(c->driver, WMH_KIND_EVENT, &ev, sizeof(ev), 0, 0);
                {
                    struct wmh_event q = { .type = WMH_EV_QUERY_MIN_MAX_INFO, .window = i };
                    send_to(c->driver, WMH_KIND_EVENT, &q, sizeof(q), 0, 0);
                }
            }
        fflush(stdout);
    }
    for (int ci = 0; ci < nconns; ci++)
        for (uint32_t i = 1; i < MAXW; i++)
            if (conns[ci].frames[i]) printf("host: %s window %u (%s): %u frames\n", conns[ci].exe, i, conns[ci].titles[i], conns[ci].frames[i]);
    for (int ci = 0; ci < nconns; ci++)
        for (uint32_t i = 1; i < MAXW; i++)
            if (conns[ci].view_frames[i]) printf("host: %s view %u: %u frames\n", conns[ci].exe, i, conns[ci].view_frames[i]);
    return 0;
}
