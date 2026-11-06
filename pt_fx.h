/* SPDX-License-Identifier: GPL-3.0-only */
/* Stereo effects for the voice: a chorus, a ping-pong delay and a small reverb. The voice is mono; the chorus (two
 * slowly swept delay taps, one to each side) is what makes it wide. Float, no libm, no allocation (~110 KiB of RAM:
 * the delay's 16-bit lines and the reverb's combs); builds on any computer. */
#ifndef PT_FX_H
#define PT_FX_H
#include "pt_engine.h"

#define FX_CH_N   2048u                             /* the chorus line: 46 ms */
#define FX_DL_N   16384u                            /* the delay lines: up to 370 ms */
#define FX_COMBS  4
#define FX_APS    2

static const uint16_t FX_COMB_LEN[2][FX_COMBS] = {{1116, 1188, 1277, 1356}, {1139, 1211, 1300, 1379}};   /* Freeverb's, the right side spread */
static const uint16_t FX_AP_LEN[2][FX_APS] = {{556, 441}, {579, 464}};

typedef struct {
    /* controls, 0..1 (the delay's time in ms) */
    volatile float chorus, delay_mix, reverb_mix, delay_ms;

    float ch_buf[FX_CH_N];
    uint32_t ch_w;
    float ph1, ph2;                                 /* the two sweeps */

    int16_t dl[2][FX_DL_N];
    uint32_t dw;
    float dly_cur;                                  /* the delay's time now (samples), sliding to the asked time */

    float comb[2][FX_COMBS][1400];
    float comb_f[2][FX_COMBS];
    uint16_t comb_i[2][FX_COMBS];
    float ap[2][FX_APS][600];
    uint16_t ap_i[2][FX_APS];
    float tiny;                                     /* keeps the tails out of the denormals */
} pt_fx_t;

static void pt_fx_init(pt_fx_t *f)
{
    uint32_t i;
    uint8_t *z = (uint8_t *)f;
    for (i = 0; i < sizeof *f; i++) z[i] = 0;
    f->chorus = 0.3f; f->delay_mix = 0.0f; f->reverb_mix = 0.25f; f->delay_ms = 280.0f;
    f->ph2 = 1.7f;
    f->dly_cur = f->delay_ms * 44.1f;
    f->tiny = 1e-18f;
}

/* x: a mono sample (-1..1); out: left and right, which may pass 1 (the caller clips) */
static inline void pt_fx_step(pt_fx_t *f, float x, float *outl, float *outr)
{
    float l = x, r = x, cm = f->chorus, dm = f->delay_mix, rm = f->reverb_mix;
    float wl = 0.0f, wr = 0.0f;

    /* ---- chorus: two taps, each swept by its own slow sine, 12 and 15 ms +- 4 ms */
    if (cm > 0.001f) {
        float d1, d2, p1, p2, a, b;
        uint32_t i1, i2;
        f->ch_buf[f->ch_w & (FX_CH_N - 1u)] = x;
        f->ph1 += 0.0000997f;                       /* 0.7 Hz */
        f->ph2 += 0.0001325f;                       /* 0.93 Hz: never in step with the first */
        if (f->ph1 > 6.2831853f) f->ph1 -= 6.2831853f;
        if (f->ph2 > 6.2831853f) f->ph2 -= 6.2831853f;
        d1 = (12.0f + 4.0f * pt_sinf(f->ph1)) * 44.1f;
        d2 = (15.0f + 4.0f * pt_sinf(f->ph2)) * 44.1f;
        p1 = (float)f->ch_w - d1; p2 = (float)f->ch_w - d2;
        i1 = (uint32_t)(int32_t)pt_floorf(p1); i2 = (uint32_t)(int32_t)pt_floorf(p2);
        a = p1 - pt_floorf(p1); b = p2 - pt_floorf(p2);
        wl = f->ch_buf[i1 & (FX_CH_N - 1u)] * (1.0f - a) + f->ch_buf[(i1 + 1u) & (FX_CH_N - 1u)] * a;
        wr = f->ch_buf[i2 & (FX_CH_N - 1u)] * (1.0f - b) + f->ch_buf[(i2 + 1u) & (FX_CH_N - 1u)] * b;
        f->ch_w++;
        l = x * (1.0f - 0.35f * cm) + wl * (0.85f * cm);
        r = x * (1.0f - 0.35f * cm) + wr * (0.85f * cm);
    }

    /* ---- ping-pong delay: the voice goes into the left line, the lines feed each other */
    if (dm > 0.001f) {
        float want = f->delay_ms * 44.1f, rl, rr, p, fr;
        uint32_t i0;
        f->dly_cur += pt_clamp(want - f->dly_cur, -0.5f, 0.5f);              /* a change of time slides: no click */
        p = (float)f->dw - f->dly_cur;
        i0 = (uint32_t)(int32_t)pt_floorf(p);
        fr = p - pt_floorf(p);
        rl = ((float)f->dl[0][i0 & (FX_DL_N - 1u)] * (1.0f - fr) + (float)f->dl[0][(i0 + 1u) & (FX_DL_N - 1u)] * fr) * (1.0f / 16384.0f);
        rr = ((float)f->dl[1][i0 & (FX_DL_N - 1u)] * (1.0f - fr) + (float)f->dl[1][(i0 + 1u) & (FX_DL_N - 1u)] * fr) * (1.0f / 16384.0f);
        {
            float wl_in = x * 0.7f + rr * 0.5f, wr_in = rl * 0.5f;
            wl_in = pt_clamp(wl_in, -1.99f, 1.99f); wr_in = pt_clamp(wr_in, -1.99f, 1.99f);
            f->dl[0][f->dw & (FX_DL_N - 1u)] = (int16_t)(wl_in * 16384.0f);
            f->dl[1][f->dw & (FX_DL_N - 1u)] = (int16_t)(wr_in * 16384.0f);
        }
        f->dw++;
        l += rl * dm;
        r += rr * dm;
    } else {
        f->dl[0][f->dw & (FX_DL_N - 1u)] = 0;                                 /* (the tails die while it is off) */
        f->dl[1][f->dw & (FX_DL_N - 1u)] = 0;
        f->dw++;
    }

    /* ---- reverb: Freeverb's combs and allpasses, fed by the dry-and-chorus mix */
    if (rm > 0.001f) {
        float in = (l + r) * 0.09f + f->tiny, ol = 0.0f, orr = 0.0f;
        int ch, k;
        f->tiny = -f->tiny;
        for (ch = 0; ch < 2; ch++) {
            float acc = 0.0f;
            for (k = 0; k < FX_COMBS; k++) {
                float *b = f->comb[ch][k], y = b[f->comb_i[ch][k]];
                f->comb_f[ch][k] = y * 0.8f + f->comb_f[ch][k] * 0.2f;      /* damping */
                b[f->comb_i[ch][k]] = in + f->comb_f[ch][k] * 0.86f;
                if (++f->comb_i[ch][k] >= FX_COMB_LEN[ch][k]) f->comb_i[ch][k] = 0;
                acc += y;
            }
            for (k = 0; k < FX_APS; k++) {
                float *b = f->ap[ch][k], bo = b[f->ap_i[ch][k]], o = -acc + bo;
                b[f->ap_i[ch][k]] = acc + bo * 0.5f;
                if (++f->ap_i[ch][k] >= FX_AP_LEN[ch][k]) f->ap_i[ch][k] = 0;
                acc = o;
            }
            if (ch) orr = acc; else ol = acc;
        }
        l += ol * rm * 1.6f;
        r += orr * rm * 1.6f;
    }
    *outl = l;
    *outr = r;
}

#endif
