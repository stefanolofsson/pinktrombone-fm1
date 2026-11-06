/* SPDX-License-Identifier: GPL-3.0-only */
/* The picture of the voice: the throat and mouth as a bent tube (from the vocal folds up the back of the throat and
 * out through the lips), with the tongue's body, the teeth, the lips, the nose with its velum, the folds opening and
 * closing in time with the voice, and a scope of the sound. Software-rendered into a 240 x GH frame buffer that the module
 * blits (RGB565, byte-swapped as the panel takes it). Float, no libm; builds on any computer (host/modtest.c). */
#ifndef PT_GFX_H
#define PT_GFX_H
#include "pt_engine.h"

#define GW 240
#define GH 136
#define G_CX   88.0f                                /* the bend's centre */
#define G_CY   88.0f
#define G_RO   70.0f                                /* the roof / back wall's radius */
#define G_S    13.0f                                /* pixels per unit of diameter */
#define G_STEP 14.0f                                /* pixels per segment along the straight part (the mouth) */
#define G_ARC  38.0f                                /* segments on the bend: 0..38; the rest is the mouth */
#define G_NST  91                                   /* stations: s = -2 .. 43 in steps of 0.5 */

static uint16_t *g_fb;

static inline uint16_t g_rgb(int r, int g, int b)
{
    uint16_t c;
    r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
    c = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return (uint16_t)((c >> 8) | (c << 8));
}
static inline uint16_t g_mix(int r0, int g0, int b0, int r1, int g1, int b1, float t)
{
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return g_rgb(r0 + (int)((float)(r1 - r0) * t), g0 + (int)((float)(g1 - g0) * t), b0 + (int)((float)(b1 - b0) * t));
}

static inline void g_px(int x, int y, uint16_t c) { if ((unsigned)x < GW && (unsigned)y < GH) g_fb[y * GW + x] = c; }

static void g_span(int y, int xa, int xb, uint16_t c)
{
    int x;
    if ((unsigned)y >= GH) return;
    if (xa > xb) { x = xa; xa = xb; xb = x; }
    if (xa < 0) xa = 0;
    if (xb >= GW) xb = GW - 1;
    for (x = xa; x <= xb; x++) g_fb[y * GW + x] = c;
}

static void g_tri(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c)
{
    float t;
    int y, ya, yb;
    if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y1 > y2) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y2 - y0 < 0.01f) { g_span((int)(y0 + 0.5f), (int)pt_fminf(x0, pt_fminf(x1, x2)), (int)(pt_fmaxf(x0, pt_fmaxf(x1, x2)) + 0.5f), c); return; }
    ya = (int)pt_floorf(y0 + 0.5f);
    yb = (int)pt_floorf(y2 + 0.5f);
    for (y = ya; y <= yb; y++) {
        float fy = (float)y, xa, xb, ta, tb;
        ta = (fy - y0) / (y2 - y0);                      /* (clamped: the rows round outside the triangle by up to half a pixel) */
        ta = ta < 0.0f ? 0.0f : ta > 1.0f ? 1.0f : ta;
        xa = x0 + (x2 - x0) * ta;
        if (fy < y1) {
            tb = y1 - y0 < 0.01f ? 1.0f : (fy - y0) / (y1 - y0);
            tb = tb < 0.0f ? 0.0f : tb > 1.0f ? 1.0f : tb;
            xb = x0 + (x1 - x0) * tb;
        } else {
            tb = y2 - y1 < 0.01f ? 1.0f : (fy - y1) / (y2 - y1);
            tb = tb < 0.0f ? 0.0f : tb > 1.0f ? 1.0f : tb;
            xb = x1 + (x2 - x1) * tb;
        }
        if (xa > xb) { t = xa; xa = xb; xb = t; }
        g_span(y, (int)pt_floorf(xa), (int)pt_ceilf(xb), c);
    }
}

static void g_quad(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy, uint16_t c)
{
    g_tri(ax, ay, bx, by, cx, cy, c);
    g_tri(ax, ay, cx, cy, dx, dy, c);
}

static void g_disc(float cx, float cy, float r, uint16_t c)
{
    int y, y0 = (int)pt_floorf(cy - r), y1 = (int)pt_ceilf(cy + r);
    for (y = y0; y <= y1; y++) {
        float dy = (float)y - cy, w = r * r - dy * dy;
        if (w < 0.0f) continue;
        w = pt_sqrtf(w);
        g_span(y, (int)(cx - w + 0.5f), (int)(cx + w), c);
    }
}

static void g_line(float x0, float y0, float x1, float y1, float r, uint16_t c)     /* a round-ended thick line */
{
    float dx = x1 - x0, dy = y1 - y0, len = pt_sqrtf(dx * dx + dy * dy);
    int n = (int)(len / (r > 0.8f ? r * 0.7f : 0.7f)) + 1, i;
    for (i = 0; i <= n; i++) {
        float t = (float)i / (float)n;
        if (r < 0.8f) g_px((int)(x0 + dx * t + 0.5f), (int)(y0 + dy * t + 0.5f), c);
        else g_disc(x0 + dx * t, y0 + dy * t, r, c);
    }
}

static void g_rect(int x, int y, int w, int h, uint16_t c)
{
    int j;
    for (j = 0; j < h; j++) g_span(y + j, x, x + w - 1, c);
}

/* ------------------------------------------------------------------ geometry */
typedef struct { float ox, oy, ix, iy, nx, ny; } g_st_t;      /* the roof's point, the tongue's point, the outward normal */

static float g_diam(const pt_t *p, float s)                 /* the diameter at s, Catmull-Rom through the segments */
{
    int i;
    float t, d0, d1, d2, d3;
    if (s < 0.0f) s = 0.0f;
    if (s > (float)(PT_N - 1)) s = (float)(PT_N - 1);
    i = (int)s;
    t = s - (float)i;
    d1 = p->diameter[i];
    d0 = p->diameter[i > 0 ? i - 1 : 0];
    d2 = p->diameter[i < PT_N - 1 ? i + 1 : PT_N - 1];
    d3 = p->diameter[i < PT_N - 2 ? i + 2 : PT_N - 1];
    d1 = d1 + 0.5f * t * (d2 - d0 + t * (2.0f * d0 - 5.0f * d1 + 4.0f * d2 - d3 + t * (3.0f * (d1 - d2) + d3 - d0)));
    return d1 < 0.0f ? 0.0f : d1;
}

static void g_station(const pt_t *p, float s, g_st_t *o)
{
    float d, th, c, sn, r;
    if (s < 0.0f) {                                         /* the windpipe: straight down, widening */
        d = g_diam(p, 0.0f) + (-s) * 0.4f;
        o->ox = G_CX - G_RO; o->oy = G_CY - s * 16.0f;
        o->ix = o->ox + d * G_S; o->iy = o->oy;
        o->nx = -1.0f; o->ny = 0.0f;
    } else if (s <= G_ARC) {                                /* the bend: a quarter turn, from straight up to straight right */
        d = g_diam(p, s);
        th = PT_PI - (PT_PI * 0.5f) * s / G_ARC;
        c = pt_cosf(th); sn = pt_sinf(th);
        r = G_RO - d * G_S;
        o->ox = G_CX + G_RO * c; o->oy = G_CY - G_RO * sn;
        o->ix = G_CX + r * c; o->iy = G_CY - r * sn;
        o->nx = c; o->ny = -sn;
    } else {                                                /* the mouth: straight out */
        d = g_diam(p, s);
        o->ox = G_CX + (s - G_ARC) * G_STEP; o->oy = G_CY - G_RO;
        o->ix = o->ox; o->iy = o->oy + d * G_S;
        o->nx = 0.0f; o->ny = -1.0f;
    }
}

/* the nose's path: from the roof at the velum, out, over the top and forward. u 0..1 */
static void g_nose_pt(float u, float *x, float *y)
{
    const float th0 = PT_PI - (PT_PI * 0.5f) * (float)PT_NOSE_START / G_ARC, rn = G_RO + 11.0f;
    if (u < 0.55f) {
        float th = th0 - (th0 - PT_PI * 0.5f) * (u / 0.55f);
        *x = G_CX + rn * pt_cosf(th);
        *y = G_CY - rn * pt_sinf(th);
    } else {
        *x = G_CX + (u - 0.55f) / 0.45f * 66.0f;
        *y = G_CY - rn;
    }
}

/* ------------------------------------------------------------------ the picture */
/* ring: the last output samples (a power of two long, ring_n), ring_w the count written; level 0..1 */
static void pt_gfx_draw(uint16_t *fb, const pt_t *p, float level, const int16_t *ring, uint32_t ring_n, uint32_t ring_w)
{
    static g_st_t st[G_NST];
    int k, x, y;
    uint16_t c_bg0, c_roof, c_mass, c_la, c_lb, c_wo, c_wi, c_lip, c_teeth;
    float open, gap_w;
    g_fb = fb;
    level = level < 0.0f ? 0.0f : level > 1.0f ? 1.0f : level;

    /* the background: a deep plum, a little lighter towards the top */
    for (y = 0; y < GH; y++) {
        uint16_t c = g_mix(30, 11, 26, 11, 4, 11, (float)y / (float)GH);
        for (x = 0; x < GW; x++) fb[y * GW + x] = c;
    }
    c_bg0 = g_rgb(11, 4, 11);
    (void)c_bg0;
    c_roof = g_rgb(104, 40, 70);
    c_mass = g_rgb(150, 60, 98);
    c_la = g_mix(48, 9, 28, 160, 46, 98, level);
    c_lb = g_mix(74, 15, 42, 255, 108, 160, level);
    c_wo = g_rgb(222, 98, 150);
    c_wi = g_rgb(255, 150, 192);
    c_lip = g_rgb(255, 92, 128);
    c_teeth = g_rgb(240, 232, 226);

    for (k = 0; k < G_NST; k++) g_station(p, -2.0f + 0.5f * (float)k, &st[k]);

    /* the nose, behind the head: a tube over the roof, the velum a flap where it joins */
    {
        float vel = p->velum_d, x0, y0, x1, y1, open_n = vel > 0.06f ? 1.0f : 0.0f;
        uint16_t cn = g_mix(52, 14, 38, 150, 70, 130, open_n * (0.25f + 0.75f * level));
        const int n = 40;
        for (k = 0; k <= n; k++) {
            float u = (float)k / (float)n, w = 2.0f + 2.4f * pt_sqrtf(p->nose_A[(int)(u * (float)(PT_NM - 1) + 0.5f)]);
            g_nose_pt(u, &x0, &y0);
            g_disc(x0, y0, w + 1.8f, c_roof);
        }
        for (k = 0; k <= n; k++) {
            float u = (float)k / (float)n, w = 2.0f + 2.4f * pt_sqrtf(p->nose_A[(int)(u * (float)(PT_NM - 1) + 0.5f)]);
            g_nose_pt(u, &x0, &y0);
            g_disc(x0, y0, w, cn);
        }
        g_nose_pt(1.0f, &x0, &y0);
        g_disc(x0 + 3.0f, y0, 2.6f, g_rgb(255, 130, 170));                           /* the nostril */
        {
            int vi = PT_NOSE_START * 2;                                             /* the velum, at the roof */
            g_nose_pt(0.0f, &x1, &y1);
            g_line(st[vi].ox, st[vi].oy, x1, y1, 1.0f + vel * 14.0f, vel > 0.06f ? g_mix(60, 18, 44, 190, 90, 150, level) : c_roof);
        }
    }

    /* tissue: the roof above, the tongue's body and the neck below */
    for (k = 0; k + 1 < G_NST; k++) {
        g_st_t *a = &st[k], *b = &st[k + 1];
        g_quad(a->ox, a->oy, a->ox + a->nx * 8.0f, a->oy + a->ny * 8.0f, b->ox + b->nx * 8.0f, b->oy + b->ny * 8.0f, b->ox, b->oy, c_roof);
        {
            float sa = -2.0f + 0.5f * (float)k, ta = sa > G_ARC ? 24.0f - (sa - G_ARC) * 3.0f : 24.0f;   /* the body tapers to the lower lip */
            float tb = sa + 0.5f > G_ARC ? 24.0f - (sa + 0.5f - G_ARC) * 3.0f : 24.0f;
            g_quad(a->ix, a->iy, a->ix - a->nx * ta, a->iy - a->ny * ta, b->ix - b->nx * tb, b->iy - b->ny * tb, b->ix, b->iy, c_mass);
        }
    }
    /* the air: a dark channel, a brighter core, both glowing with the voice */
    for (k = 0; k + 1 < G_NST; k++) {
        g_st_t *a = &st[k], *b = &st[k + 1];
        float ga = pt_sqrtf((a->ox - a->ix) * (a->ox - a->ix) + (a->oy - a->iy) * (a->oy - a->iy));
        float gb = pt_sqrtf((b->ox - b->ix) * (b->ox - b->ix) + (b->oy - b->iy) * (b->oy - b->iy));
        float ea = pt_fminf(2.5f, ga * 0.3f), eb = pt_fminf(2.5f, gb * 0.3f);
        if (ga < 0.4f && gb < 0.4f) continue;                                         /* closed */
        g_quad(a->ox, a->oy, a->ix, a->iy, b->ix, b->iy, b->ox, b->oy, c_la);
        if (ga > 2.0f && gb > 2.0f)
            g_quad(a->ox - a->nx * ea, a->oy - a->ny * ea, a->ix + a->nx * ea, a->iy + a->ny * ea,
                   b->ix + b->nx * eb, b->iy + b->ny * eb, b->ox - b->nx * eb, b->oy - b->ny * eb, c_lb);
    }
    /* the walls */
    for (k = 0; k + 1 < G_NST; k++) {
        g_line(st[k].ox, st[k].oy, st[k + 1].ox, st[k + 1].oy, 1.2f, c_wo);
        g_line(st[k].ix, st[k].iy, st[k + 1].ix, st[k + 1].iy, 1.7f, c_wi);
    }

    /* teeth and lips at the mouth (stations for s = 40 .. 43) */
    {
        g_st_t *t = &st[(int)((40.5f + 2.0f) * 2.0f)], *l = &st[G_NST - 1], *m = &st[(int)((41.5f + 2.0f) * 2.0f)];
        float gap = t->iy - t->oy;
        if (gap > 12.0f) {
            g_rect((int)t->ox - 2, (int)t->oy + 1, 5, 6, c_teeth);
            g_rect((int)t->ix - 2, (int)t->iy - 6, 5, 6, c_teeth);
        }
        g_line(m->ox, m->oy, l->ox + 3.0f, l->oy + 1.0f, 3.0f, c_lip);
        g_line(m->ix, m->iy, l->ix + 3.0f, l->iy - 1.0f, 3.0f, c_lip);
    }

    /* the vocal folds, at the foot of the windpipe: they close and open once a period */
    {
        g_st_t *f = &st[2 * 2 + 1];                                                  /* s = -2 + 2.5 -> well, the windpipe's foot: s = -0.5 */
        float w = f->ix - f->ox, phase = p->wave_len > 0.0f ? p->t_wave / p->wave_len : 0.0f;
        float op = 0.5f + 0.5f * pt_sinf(PT_PI * 2.0f * phase), pad;
        gap_w = p->intensity > 0.05f ? w * (0.12f + 0.88f * op * (1.0f - 0.55f * p->intensity)) : w * 0.85f;
        pad = (w - gap_w) * 0.5f;
        open = gap_w;
        if (pad > 0.3f) {
            g_disc(f->ox + 1.0f + pad * 0.5f, f->oy + 6.0f, 2.6f + pad * 0.4f, g_rgb(255, 120, 150));
            g_disc(f->ix - 1.0f - pad * 0.5f, f->oy + 6.0f, 2.6f + pad * 0.4f, g_rgb(255, 120, 150));
        }
    }
    (void)open;

    /* a consonant's pinch: an arrow on the roof where it is */
    if (p->cons_on > 0.5f) {
        g_st_t *a = &st[(int)((p->cons_idx + 2.0f) * 2.0f + 0.5f)];
        float px = -a->ny, py = a->nx;
        g_tri(a->ox + a->nx * 3.0f, a->oy + a->ny * 3.0f,
              a->ox + a->nx * 12.0f + px * 4.5f, a->oy + a->ny * 12.0f + py * 4.5f,
              a->ox + a->nx * 12.0f - px * 4.5f, a->oy + a->ny * 12.0f - py * 4.5f, g_rgb(255, 255, 255));
    }

    /* the scope: the sound, from a rising zero crossing */
    {
        const int sx = 168, sy = 70, sw = 68, sh = 54;
        uint16_t cf = g_rgb(70, 28, 52), cg = g_rgb(44, 18, 34), cw = g_mix(160, 60, 110, 255, 170, 205, level);
        int i, start = 0, py0 = 0, have = 0;
        g_rect(sx - 1, sy - 1, sw + 2, sh + 2, cf);
        g_rect(sx, sy, sw, sh, g_rgb(16, 6, 14));
        g_span(sy + sh / 2, sx, sx + sw - 1, cg);
        if (ring_w > ring_n) {
            uint32_t base = ring_w - 480u;
            for (i = 0; i < 200; i++)
                if (ring[(base + (uint32_t)i) & (ring_n - 1u)] <= 0 && ring[(base + (uint32_t)i + 1u) & (ring_n - 1u)] > 0) { start = i; break; }
            for (i = 0; i < sw; i++) {
                int v = ring[(base + (uint32_t)start + (uint32_t)i * 4u) & (ring_n - 1u)];
                int yy = sy + sh / 2 - v * (sh / 2 - 3) / 16000;
                yy = yy < sy + 1 ? sy + 1 : yy > sy + sh - 2 ? sy + sh - 2 : yy;
                if (have) g_line((float)(sx + i - 1), (float)py0, (float)(sx + i), (float)yy, 0.5f, cw);
                py0 = yy; have = 1;
            }
        }
    }
}

#endif
