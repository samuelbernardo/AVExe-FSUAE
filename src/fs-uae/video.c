#include <stdio.h>
#include <stdlib.h>
#include <uae/uae.h>
#include <fs/emu.h>
#include <fs/i18n.h>
#include "fs-uae.h"

//#define MAX_ZOOM_MODES 5
static int g_zoom_mode = 0;
static int g_zoom_border = 0;
static int g_last_refresh_rate = 0;

typedef struct zoom_mode {
    char *name;
    char *cname;
    int x;
    int y;
    int w;
    int h;
} zoom_mode;

#define CUSTOM_ZOOM_MODE 5

static zoom_mode g_zoom_modes[] = {
    /// TRANSLATORS: In context "Zoom: Auto"
    { N_("Auto"), NULL, 0, 0, 0, 0 },
    /// TRANSLATORS: In context "Zoom: Full Frame"
    { N_("Full Frame"), "full", 0, 0, 752, 572 },
    { "640x512", NULL, 74, 36, 640, 512 },
    { "640x480", NULL, 74, 36, 640, 480 },
    { "640x400", NULL, 74, 36, 640, 400 },
    { NULL, NULL, 0, 0, 0, 0 },
    { NULL, NULL, 0, 0, 0, 0 },
};

int g_fs_uae_video_zoom = 1;

struct WindowOverride {
    int sx;
    int sy;
    int sw;
    int sh;
    int dx;
    int dy;
    int dw;
    int dh;

    int ssx;
    int ssy;
    int ssw;
    int ssh;
    struct WindowOverride* next;
};

static struct WindowOverride* g_window_override = NULL;
static struct WindowOverride* g_last_window_override = NULL;

static int g_frame_seq_no = 0;
static fs_emu_video_buffer *g_buffer = NULL;
static int g_remember_last_screen = 0;

static int g_use_rtg_scanlines = 0;
static int g_last_seen_mode_rtg = 0;

int read_window_override_int(const char* s, int* pos, int* out) {
    char temp[4];
    int read = 0;
    while(s[*pos] == ' ') ++(*pos);
    while (read < 3) {
        temp[read] = s[*pos];
        ++(*pos);
        ++read;
        char c= s[*pos];
        if (c >= '0' && c <= '9') {
            continue;
        }
        temp[read] = '\0';
        if (read == 1) {
            if (temp[0] == '*') {
                *out = -1;
                return 1;
            }
        }
        *out = atoi(temp);
        return 1;
    }
    // read failed
    return 0;
}

int read_window_override(const char* s, int* pos) {
    while(s[*pos] == ' ') ++(*pos);
    int sx, sy, sw, sh;
    int dx, dy, dw, dh;
    int ssx = 0, ssy = 0, ssw = 0, ssh = 0;
    if (!read_window_override_int(s, pos, &sx)) return 0;
    if (!read_window_override_int(s, pos, &sy)) return 0;
    if (!read_window_override_int(s, pos, &sw)) return 0;
    if (!read_window_override_int(s, pos, &sh)) return 0;
    while(s[*pos] == ' ') ++(*pos);
    if (s[(*pos)] == '[') {
        ++(*pos);
        if (!read_window_override_int(s, pos, &ssx)) return 0;
        if (!read_window_override_int(s, pos, &ssy)) return 0;
        if (!read_window_override_int(s, pos, &ssw)) return 0;
        if (!read_window_override_int(s, pos, &ssh)) return 0;
        while(s[*pos] == ' ') ++(*pos);
        if (!s[(*pos)++] == ']') return 0;
    }

    if (!s[(*pos)++] == '=') return 0;
    if (s[(*pos)++] == '>') ++(*pos);
    if (!read_window_override_int(s, pos, &dx)) return 0;
    if (!read_window_override_int(s, pos, &dy)) return 0;
    if (!read_window_override_int(s, pos, &dw)) return 0;
    if (!read_window_override_int(s, pos, &dh)) return 0;
    fs_emu_log("viewport transformation: %3d %3d %3d %3d => %3d %3d %3d %3d\n",
            sx, sy, sw, sh, dx, dy, dw, dh);
    struct WindowOverride* wo = (struct WindowOverride*)
            malloc(sizeof(struct WindowOverride));
    wo->sx = sx;
    wo->sy = sy;
    wo->sw = sw;
    wo->sh = sh;
    wo->dx = dx;
    wo->dy = dy;
    wo->dw = dw;
    wo->dh = dh;

    wo->ssx = ssx;
    wo->ssy = ssy;
    wo->ssw = ssw;
    wo->ssh = ssh;
    wo->next = NULL;

    if (g_last_window_override == NULL) {
        g_window_override = wo;
    }
    else {
        g_last_window_override->next = wo;
    }
    g_last_window_override = wo;
    return 1;
}

void init_window_overrides() {
    const char *s = fs_config_get_const_string("viewport");
    if (s == NULL) {
        return;
    }
    int pos = 0;
    while (1) {
        int result = read_window_override(s, &pos);
        if (!result) {
            fs_emu_log("error parsing wiewport transformation\n");
        }
        while(s[pos] == ' ') ++(pos);
        int c = s[(pos)++];
        if (c == ';') {
            continue;
        }
        else if (c == ',') {
            continue;
        }
        else if (c == '\0') {
            break;
        }
        else {
            fs_emu_warning("Unexpected byte (%d) while parsing "
                    "viewport option\n", c);
            return;
        }
    }
}

static int ucx = 0, ucy = 0, ucw = 0, uch = 0;
static int rd_width, rd_height;

static int modify_coordinates(int *cx, int *cy, int *cw, int *ch) {
    int changed = 0;
    int ocx = *cx;
    int ocy = *cy;
    int ocw = *cw;
    int och = *ch;
    if (*cx == 114 && *cy == 96 && *cw == 560 && *ch == 384) {
        fs_log("* amiga 600 kickstart screen?\n");
        *cx = 74; *cy = 92; *cw = 640; *ch = 400; changed = 1;
    }
    else if (*cx == 114 && *cy == 99 && *cw == 560 && *ch == 384) {
        fs_log("* amiga 1200 kickstart screen?\n");
        *cx = 74; *cy = 92; *cw = 640; *ch = 400; changed = 1;
    }
    // The following interfered with autoscaling for Jim Power
    /*
    else if (*cx == 74 && *cy == 30 && *cw == 640 && *ch == 518) {
        fs_log("* workbench 1.3/2.0 screen?\n");
        *cx = 74; *cy = 36; *cw = 640; *ch = 512; changed = 1;
    }
    else if (*cx == 74 && *cy == 28 && *cw == 640 && *ch == 520) {
        fs_log("* workbench 1.3/2.0 screen?\n");
        *cx = 74; *cy = 36; *cw = 640; *ch = 512; changed = 1;
    }
    */
    /*
    else if (*cx == 6 && *cy == 36 && *cw == 724 && *ch == 512) {
        fs_log("* workbench screen with too much border?\n");
        *cx = 74; *cy = 36; *cw = 640; *ch = 512; changed = 1;
    }
    */
    else if (*cx == 6 && *cy == 6 && *cw == 724 && *ch == 566) {
        fs_log("* workbench screen with overscan incorrectly placed?\n");
        *cx = 2; *cy = 6; *cw = 724; *ch = 566; changed = 1;
    }
    else if (*cx == 10 && *cy == 7 && *cw == 716 && *ch == 566) {
        fs_log("* amiga cd32 boot screen?\n");
        *cx = 16; *cy = 6; *cw = 704; *ch = 566; changed = 1;
    }
    else if (*cx == 10 && *cy == 6 && *cw == 716 && *ch == 566) {
        fs_log("* amiga cd32 boot screen?\n");
        *cx = 16; *cy = 6; *cw = 704; *ch = 566; changed = 1;
    }
    else if (*cx == 6 && *cy == 96 && *cw == 724 && *ch == 476) {
        fs_log("* amiga cd32 boot screen (booting CD)\n");
        *cx = 16; *cy = 6; *cw = 704; *ch = 566; changed = 1;
    }
    else if (*cx == 10 && *cy == 96 && *cw == 716 && *ch == 476) {
        fs_log("* amiga cd32 boot screen (booting CD)\n");
        *cx = 16; *cy = 6; *cw = 704; *ch = 566; changed = 1;
    }
    else if (*cx == 6 && *cy == 82 && *cw == 724 && *ch == 490) {
        fs_log("* amiga cd32 boot screen (booting Arcade Pool CD)\n");
        *cx = 16; *cy = 6; *cw = 704; *ch = 566; changed = 1;
    }
    else if (*cx + *cw == 698 && *cy == 6 && *ch == 566) {
        fs_log("* amiga cd32 menu\n");
        *cx = 16; *cy = 6; *cw = 704; *ch = 566; changed = 1;
    }
    if (changed) {
        fs_log("* %3d %3d %3d %3d [ %3d %3d %3d %3d ]\n",
                *cx, *cy, *cw, *ch, ocx, ocy, ocw, och);
        printf("* %3d %3d %3d %3d [ %3d %3d %3d %3d ]\n",
                *cx, *cy, *cw, *ch, ocx, ocy, ocw, och);
    }
    return changed;
}

#define SUBSCAN

#ifdef SUBSCAN
static void narrow_rect(RenderData* rd, int *nx, int *ny, int *nw, int *nh) {
    if (rd->bpp != 4) {
        // not implemented for 16-bit video.
        return;
    }
#if 0
    int64_t t1 = fs_get_monotonic_time();
#endif
    int x = *nx;
    int y = *ny;
    int w = *nw;
    int h = *nh;

    uint32_t *ibuffer = (uint32_t *) rd->pixels;
    uint32_t *p1, *p;
    uint32_t cmpval;

    p1 = ibuffer + y * rd->width + x;
    cmpval = 0;
    for (int i = 0; i < h; i++) {
        cmpval += *p1;
    }
    while (w > 0) {
        p = p1;
        uint32_t val = 0;
        for (int i = 0; i < h; i++) {
            val = val + *p;
            p += rd->width;
        }
        if (val != cmpval) {
            break;
        }
        x = x + 1;
        w = w - 1;
        p1 += 1;
    }

    p1 = ibuffer + y * rd->width + x + w - 1;
    cmpval = 0;
    for (int i = 0; i < h; i++) {
        cmpval += *p1;
    }
    while (w > 0) {
        p = p1;
        uint32_t val = 0;
        for (int i = 0; i < h; i++) {
            val = val + *p;
            p += rd->width;
        }
        if (val != cmpval) {
            break;
        }
        w = w - 1;
        p1 -= 1;
    }

    p1 = ibuffer + y * rd->width + x;
    cmpval = 0;
    for (int i = 0; i < w; i++) {
        cmpval += *p1;
    }
    while (h > 0) {
        p = p1;
        uint32_t val = 0;
        for (int i = 0; i < w; i++) {
            val = val + *p;
            p++;
        }
        if (val != cmpval) {
            break;
        }
        y = y + 1;
        h = h - 1;
        p1 += rd->width;
    }

    p1 = ibuffer + (y + h - 1) * rd->width + x;
    cmpval = 0;
    for (int i = 0; i < w; i++) {
        cmpval += *p1;
    }
    while (h > 0) {
        p = p1;
        uint32_t val = 0;
        for (int i = 0; i < w; i++) {
            val = val + *p;
            p++;
        }
        if (val != cmpval) {
            break;
        }
        h = h - 1;
        p1 -= rd->width;
    }

    static int px = 0, py = 0, pw = 0, ph = 0;
    if (x != px || y != py || w != pw || h != ph) {
        fs_log(" sub: %3d %3d %3d %3d\n", x, y, w, h);
        printf(" sub: %3d %3d %3d %3d\n", x, y, w, h);
        px = x;
        py = y;
        pw = w;
        ph = h;
    }

    *nx = x;
    *ny = y;
    *nw = w;
    *nh = h;
#if 0
    int64_t t2 = fs_get_monotonic_time();
    int64_t diff = t2 - t1;
    printf("in %d us\n", (int) diff);
#endif
}
#endif

static void render_screen(RenderData* rd) {
#if 0
    static int64_t last_time = 0;
    int64_t t = fs_emu_monotonic_time();
    int dt = (int) (t - last_time);
    // if we loose a frame in vsync mode, we should have a delay of about
    // 40 ms since last frame
    if (dt > 35 * 1000) {
        printf("fs-uae:render_screen dt %0.2f\n", dt / 1000.0);
    }
    last_time = t;
#endif

    rd_width = rd->width;
    rd_height = rd->height;

    g_buffer->seq = g_frame_seq_no++;
    g_buffer->width = rd_width;
    g_buffer->height = rd_height;
    g_buffer->flags = 0;
    if (rd->flags & AMIGA_VIDEO_RTG_MODE) {
        g_buffer->flags = FS_EMU_FORCE_VIEWPORT_CROP_FLAG;
        if (g_use_rtg_scanlines == 0) {
            g_buffer->flags |= FS_EMU_NO_SCANLINES_FLAG;
        }
    }
    memcpy(g_buffer->line, rd->line, AMIGA_MAX_LINES);
    fs_emu_video_buffer_update_lines(g_buffer);

    static int lastcx = 0, lastcy = 0, lastcw = 0, lastch = 0;
    static int lastsubscan = 0;

    // crop rectangle for autoscale
    int cx = rd->limit_x;
    int cy = rd->limit_y;
    int cw = rd->limit_w;
    int ch = rd->limit_h;

    // normalize coordinates to high-resolution, interlaced pixels
    // before viewport transformations are applied

    int hshift = (rd->flags & AMIGA_VIDEO_LOW_RESOLUTION) ? 1 : 0;
    int vshift = (!(rd->flags & AMIGA_VIDEO_LINE_DOUBLING)) ? 1 : 0;

    cx <<= hshift;
    cw <<= hshift;
    cy <<= vshift;
    ch <<= vshift;

    int cchange = lastcx != cx || lastcy != cy || lastcw != cw || lastch != ch;
    if (cchange || lastsubscan) {
        if (cchange) {
            lastcx = cx;
            lastcy = cy;
            lastcw = cw;
            lastch = ch;
        }
        lastsubscan = 0;
        struct WindowOverride* wo = NULL;
#ifdef SUBSCAN
        int ncx, ncy, ncw, nch;
        int have_narrowed = 0;
#endif
        if (cchange) {
            fs_log("auto: %3d %3d %3d %3d\n", cx, cy, cw, ch);
            printf("auto: %3d %3d %3d %3d\n", cx, cy, cw, ch);
        }
        modify_coordinates(&cx, &cy, &cw, &ch);
        //if (!modify_coordinates(&cx, &cy, &cw, &ch)) {
        if (1) {
            wo = g_window_override;
            while (wo != NULL) {
                if ((wo->sx == -1 || wo->sx == cx) &&
                        (wo->sy == -1 || wo->sy == cy) &&
                        (wo->sw == -1 || wo->sw == cw) &&
                        (wo->sh == -1 || wo->sh == ch)) {
                    ucx = wo->dx == -1 ? cx : wo->dx;
                    ucy = wo->dy == -1 ? cy : wo->dy;
                    ucw = wo->dw == -1 ? cw : wo->dw;
                    uch = wo->dh == -1 ? ch : wo->dh;
#ifdef SUBSCAN
                    if (wo->ssx != 0) {
                        lastsubscan = 1;
                        if (!have_narrowed) {
                            ncx = cx;
                            ncy = cy;
                            ncw = cw;
                            nch = ch;
                            narrow_rect(rd, &ncx, &ncy, &ncw, &nch);
                            have_narrowed = 1;
                        }
                        if ((wo->ssx == -1 || wo->ssx == ncx) &&
                                (wo->ssy == -1 || wo->ssy == ncy) &&
                                (wo->ssw == -1 || wo->ssw == ncw) &&
                                (wo->ssh == -1 || wo->ssh == nch)) {
                            ucx = wo->dx == -1 ? ncx : wo->dx;
                            ucy = wo->dy == -1 ? ncy : wo->dy;
                            ucw = wo->dw == -1 ? ncw : wo->dw;
                            uch = wo->dh == -1 ? nch : wo->dh;
                            break;
                        }
                        // continue searching
                        wo = wo->next;
                        continue;
                    }
#endif
                    /*
                    fs_log("%3d %3d %3d %3d [ %3d %3d %3d %3d ]\n",
                            ucx, ucy, ucw, uch, cx, cy, cw, ch);
                    printf("%3d %3d %3d %3d [ %3d %3d %3d %3d ]\n",
                            ucx, ucy, ucw, uch, cx, cy, cw, ch);
                    */
                    break;
                }
                wo = wo->next;
            }
        }
        if (wo == NULL) {
            ucx = cx;
            ucy = cy;
            ucw = cw;
            uch = ch;
            /*
            fs_log("%3d %3d %3d %3d\n", ucx, ucy, ucw, uch);
            printf("%3d %3d %3d %3d\n", ucx, ucy, ucw, uch);
            */
        }
        static int lucx = 0, lucy = 0, lucw = 0, luch = 0;
        if (ucx != lucx || ucy != lucy || ucw != lucw || uch != luch) {
            fs_log("    = %3d %3d %3d %3d\n", ucx, ucy, ucw, uch);
            printf("    = %3d %3d %3d %3d\n", ucx, ucy, ucw, uch);
            lucx = ucx;
            lucy = ucy;
            lucw = ucw;
            luch = uch;
        }
    }
    float tx0, ty0, tx1, ty1; //source buffer coords



    fs_emu_rect crop;
    crop.x = 0;
    crop.y = 0;
    crop.w = rd_width;
    crop.h = rd_height;

    if (rd->flags & AMIGA_VIDEO_RTG_MODE) {
        // no cropping in RTG mode
    }
    else {
        if (g_fs_uae_video_zoom && ucw > 0 && uch > 0) {
            // autoscale
            if (g_zoom_mode == 0) {
                crop.x = ucx >> hshift;
                crop.w = ucw >> hshift;
                crop.y = ucy >> vshift;
                crop.h = uch >> vshift;
            }
        }
        if (g_zoom_mode > 0) {
            crop.x = g_zoom_modes[g_zoom_mode].x >> hshift;
            crop.w = g_zoom_modes[g_zoom_mode].w >> hshift;
            crop.y = g_zoom_modes[g_zoom_mode].y >> vshift;
            crop.h = g_zoom_modes[g_zoom_mode].h >> vshift;
        }
        if (g_zoom_border) {
            crop.x -= 10 >> hshift;
            crop.w += 20 >> hshift;
            crop.y -= 10 >> vshift;
            crop.h += 20 >> vshift;
        }
        if (crop.x < 0) {
            crop.x = 0;
        }
        if (crop.y < 0) {
            crop.y = 0;
        }
        if (crop.x + crop.w > rd_width) {
            crop.w = rd_width - crop.x;
        }
        if (crop.y + crop.h > rd_height) {
            crop.h = rd_height - crop.y;
        }
    }

    g_buffer->crop = crop;

    g_last_seen_mode_rtg = rd->flags & AMIGA_VIDEO_RTG_MODE;
    g_last_refresh_rate = rd->refresh_rate;
}

static void *grow_buffer(int width, int height) {
    //printf("growing buffer: %p\n", g_buffer->data);
    fs_emu_video_buffer_grow(g_buffer, width, height);
    return g_buffer->data;
}

#define TURBO_FRAME_RATE 10000

static void display_screen() {

    static int64_t last_time = 0;
    int64_t t = fs_emu_monotonic_time();
    if (last_time > 0) {
        int dt = (t - last_time) / 1000;
        //printf("%d\n", dt);
    }

    fs_emu_video_buffer_set_current(g_buffer);
    if (g_last_refresh_rate == -1) {
        if (fs_emu_get_video_frame_rate() != TURBO_FRAME_RATE) {
            fs_emu_notification(45194412, _("Warp mode enabled"));
        }
        fs_emu_set_video_frame_rate(TURBO_FRAME_RATE);
    }
    else {
        if (fs_emu_get_video_frame_rate() == TURBO_FRAME_RATE) {
            fs_emu_notification(45194412, _("Warp mode disabled"));
        }
        fs_emu_set_video_frame_rate(g_last_refresh_rate);
    }

    g_buffer = fs_emu_video_buffer_get_available(g_remember_last_screen);
    //printf("new render buffer: %p\n", g_buffer->data);
    amiga_set_render_buffer(g_buffer->data, g_buffer->size,
            !g_remember_last_screen, grow_buffer);

    last_time = fs_emu_monotonic_time();
}

static void toggle_zoom(int flags) {
    if (g_last_seen_mode_rtg) {
        fs_emu_notification(1511162016, _("Zoom is disabled in RTG mode"));
        return;
    }

    if (flags == 1) {
        g_zoom_border = !g_zoom_border;
    }
    else {
        //if (g_zoom_mode < MAX_ZOOM_MODES - 1) {
        if (g_zoom_modes[g_zoom_mode + 1].name) {
            g_zoom_mode += 1;
        }
        else {
            g_zoom_mode = 0;
        }
    }
    if (g_zoom_border) {
        fs_emu_notification(1511162016, _("Zoom: %s + Border"),
                _(g_zoom_modes[g_zoom_mode].name));
    }
    else {
        fs_emu_notification(1511162016, _("Zoom: %s"),
                _(g_zoom_modes[g_zoom_mode].name));
    }
}

void fs_uae_init_video(void) {
    fs_log("fs_uae_init_video\n");
    init_window_overrides();
    fs_emu_video_buffer_init(1024, 1024, 0);

    g_buffer = fs_emu_video_buffer_get_available(g_remember_last_screen);
    amiga_set_render_buffer(g_buffer->data, g_buffer->size,
            !g_remember_last_screen, grow_buffer);
    amiga_set_render_function(render_screen);
    amiga_set_display_function(display_screen);
    if (fs_config_get_boolean("rtg_scanlines") == 1) {
        g_use_rtg_scanlines = 1;
    }

    fs_emu_set_toggle_zoom_function(toggle_zoom);

    char *value = fs_config_get_string("zoom");
    if (value) {
        char *c = value;
        while (*c) {
            if (*c == '+') {
                if (fs_ascii_strcasecmp(c + 1, "border") == 0) {
                    g_zoom_border = 1;
                }
                *c = '\0';
                break;
            }
            c++;
        }
        zoom_mode *z = g_zoom_modes;
        int k = 0;
        while (z->name) {
            //printf(":%s:%s\n", z->name, value);
            if (fs_ascii_strcasecmp(z->name, value) == 0) {
                g_zoom_mode = k;
                break;
            }
            else if (z->cname && fs_ascii_strcasecmp(z->cname, value) == 0) {
                g_zoom_mode = k;
                break;
            }
            k++;
            z++;
        }
        free(value);
    }

    const char* cvalue = fs_config_get_const_string("theme_zoom");
    if (cvalue) {
        zoom_mode *z = g_zoom_modes + CUSTOM_ZOOM_MODE;
        //char *name = malloc(strlen(cvalue) + 1);
        int x, y, w, h;
        //printf("---------\n");
        //if (sscanf(value, "%s,%d,%d,%d,%d", &name, &x, &y, &w, &h) == 5) {
        //    printf("---------\n");
        if (sscanf(cvalue, "%d,%d,%d,%d", &x, &y, &w, &h) == 4) {
            z->name = fs_strdup("Theme");
            z->x = x;
            z->y = y;
            z->w = w;
            z->h = h;
        }
        g_zoom_mode = CUSTOM_ZOOM_MODE;
    }
}
