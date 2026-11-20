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
 */
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <IOSurface/IOSurfaceRef.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <CoreFoundation/CoreFoundation.h>
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

static void send_to(mach_port_t port, uint32_t kind, const void *payload, size_t size, mach_port_t give, int give_disp)
{
    struct { mach_msg_header_t h; mach_msg_body_t b; mach_msg_port_descriptor_t d; struct wmh_header w; char p[256]; } m;
    struct { mach_msg_header_t h; struct wmh_header w; char p[256]; } *pm = (void *)&m;
    kern_return_t kr;

    memset(&m, 0, sizeof(m));
    if (give)
    {
        m.h.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
        m.b.msgh_descriptor_count = 1;
        m.d.name = give; m.d.disposition = give_disp; m.d.type = MACH_MSG_PORT_DESCRIPTOR;
        m.w.kind = kind; memcpy(m.p, payload, size);
        m.h.msgh_size = offsetof(typeof(m), p) + ((size + 3) & ~3);
    }
    else
    {
        pm->h.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
        pm->w.kind = kind; memcpy(pm->p, payload, size);
        pm->h.msgh_size = sizeof(pm->h) + sizeof(pm->w) + ((size + 3) & ~3);
    }
    m.h.msgh_remote_port = port;
    m.h.msgh_id = WMH_MSG_ID;
    kr = mach_msg(&m.h, MACH_SEND_MSG | MACH_SEND_TIMEOUT, m.h.msgh_size, 0, 0, 1000, 0);
    if (kr) printf("host: send %u failed %x\n", kind, kr);
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
    int closed = 0;
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
