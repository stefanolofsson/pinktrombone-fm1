/* SPDX-License-Identifier: MIT */
/* Pink Trombone's voice as a C engine: the glottis (Liljencrants-Fant model) and the 44-segment
 * Kelly-Lochbaum vocal tract with its 28-segment nose, single-precision float, mono.
 *
 * A port of Neil Thapen's Pink Trombone (https://dood.al/pinktrombone/, MIT, (c) 2017 Neil Thapen),
 * made for the M-VAVE FM-1's FPU (build with -mcpu=r3 -mfprev1 -ffp-contract=off). What differs from the
 * original: it renders in 64-frame blocks (the original: 512) and the reflection coefficients are
 * interpolated in steps of 16 frames; the noise is a value noise, not simplex noise.
 *
 * No hardware, no allocation: one pt_t, pt_init(), then pt_render() from the audio interrupt and the
 * pt_set_* calls from the main loop (they only write floats, one writer each). */
#ifndef PT_ENGINE_H
#define PT_ENGINE_H
#include <stdint.h>

#define PT_N          44
#define PT_NOSE_LEN   28
#define PT_BLADE      10
#define PT_TIP        32
#define PT_LIP        39
#define PT_NOSE_START (PT_N - PT_NOSE_LEN + 1)     /* 17 (the shape's, in 44ths) */
/* The waveguide runs at half the original's resolution: 22 segments at 44.1 kHz where the original has 44 at 88.2 kHz
 * (the same 17 cm: a segment is twice as long and a step twice as long). The shape (tongue, lips, constrictions) is still
 * made in the original's 44 segments; the waveguide takes the pairs' means. Four times fewer node updates. */
#define PT_M          (PT_N / 2)
#define PT_NM         (PT_NOSE_LEN / 2)
#define PT_NOSE_M     (PT_M - PT_NM + 1)                   /* 9 */
#define PT_SR         44100.0f
#define PT_BLK        64                           /* frames per block: the tract's shape and the voice's control move once per block */
#define PT_SUB        16                           /* frames per coefficient step */
#define PT_NTRANS     8
#define PT_PI         3.14159265f
#define PT_OUT_GAIN   0.25f                        /* (the original's two steps summed, times 0.125) */

typedef struct { float pos, alive, life, strength, expo; } pt_trans_t;

typedef struct pt {
    /* ---- controls (main loop writes, audio reads) ---- */
    volatile float freq;                            /* the pitch asked for, Hz */
    volatile float tenseness;                       /* 0 (breath) .. 1 (pressed voice) */
    volatile float loudness_scale;                  /* 0..1, the note's velocity */
    volatile float vib_amount;                      /* vibrato depth, 0.005 = the original */
    volatile float velum;                           /* the nose's opening: 0.01 closed .. 0.4 open */
    volatile int gate;                              /* a key is down */
    volatile int always_voice;                      /* drone */
    volatile int shape_dirty;
    volatile float tongue_idx, tongue_dia, lip_dia; /* the tongue (12..29, 2.05..3.5), the lips (0.6 .. 3 = open) */
    volatile float cons_idx, cons_dia, cons_on;     /* a consonant's constriction; cons_on 0/1 */

    /* ---- glottis ---- */
    float t_wave, wave_len, old_f, new_f, smooth_f, old_t, new_t, total_time, intensity, loudness;
    float alpha, E0, Delta, Te, omega, epsilon, shift;

    /* ---- tract ---- */
    float R[PT_M], L[PT_M], jR[PT_M + 1], jL[PT_M + 1];
    float refl[PT_M + 1], new_refl[PT_M + 1], cur_refl[PT_M + 1];
    float diameter[PT_N], A[PT_M], rest[PT_N], target[PT_N];
    float refl_left, refl_right, refl_nose, new_refl_left, new_refl_right, new_refl_nose;
    float cur_left, cur_right, cur_nose;
    float noseR[PT_NM], noseL[PT_NM], njR[PT_NM + 1], njL[PT_NM + 1];
    float nose_refl[PT_NM], nose_A[PT_NM], velum_d;
    float glottal_refl, lip_refl, fade, damp, move_speed;
    float lip_out, nose_out;
    int last_obstruction;
    pt_trans_t trans[PT_NTRANS];
    int n_trans;
    float asp_k;                                    /* 1 - sqrt(tenseness), once per block */
    float cons_fric;                                /* the consonant's noise, ramped */

    /* ---- noise ---- */
    uint32_t rng;
    float asp_x1, asp_x2, asp_y1, asp_y2, fri_x1, fri_x2, fri_y1, fri_y2;   /* the two band-pass filters */
    float asp_b0, asp_a1, asp_a2, fri_b0, fri_a1, fri_a2;

    int block_pos;
} pt_t;


/* ------------------------------------------------------------------ own libm
 * The toolchain's libm is soft float (and its logf holds a constant the build takes for a ROM address). These are a
 * few polynomial steps each, in the FPU's single precision: ~0.1 % for sin, ~1e-4 for exp / log, plenty for a voice. */
static inline float pt_fabsf(float x) { return x < 0.0f ? -x : x; }
static inline float pt_fmaxf(float a, float b) { return a > b ? a : b; }
static inline float pt_fminf(float a, float b) { return a < b ? a : b; }
static inline float pt_floorf(float x) { int32_t i = (int32_t)x; return (float)(i - (x < (float)i ? 1 : 0)); }
static inline float pt_ceilf(float x) { int32_t i = (int32_t)x; return (float)(i + (x > (float)i ? 1 : 0)); }
static inline float pt_bits_f(uint32_t u) { union { uint32_t u; float f; } c; c.u = u; return c.f; }
static inline uint32_t pt_f_bits(float f) { union { uint32_t u; float f; } c; c.f = f; return c.u; }

static float pt_sinf(float x)                       /* any x (a few turns) */
{
    float y, ay;
    x -= 6.28318531f * pt_floorf(x * 0.159154943f + 0.5f);      /* -pi .. pi */
    y = 1.27323954f * x - 0.405284735f * x * pt_fabsf(x);       /* the parabola, then its refinement */
    ay = pt_fabsf(y);
    return y + 0.225f * (y * ay - y);
}
static inline float pt_cosf(float x) { return pt_sinf(x + 1.57079633f); }

static float pt_expf(float x)
{
    float z = x * 1.44269504f, f;
    int32_t n;
    if (z < -120.0f) return 0.0f;
    if (z > 120.0f) z = 120.0f;
    n = (int32_t)pt_floorf(z);
    f = z - (float)n;
    f = 1.0f + f * (0.693147181f + f * (0.240226507f + f * (0.0555041087f + f * 0.00961812911f)));
    return f * pt_bits_f((uint32_t)(n + 127) << 23);
}

static float pt_logf(float x)                       /* x > 0 */
{
    uint32_t b = pt_f_bits(x);
    int32_t e = (int32_t)(b >> 23 & 255u) - 127;
    float m = pt_bits_f((b & 0x7FFFFFu) | 0x3F800000u), t, t2;      /* m in 1..2 */
    t = (m - 1.0f) / (m + 1.0f);
    t2 = t * t;
    return 0.693147181f * (float)e + 2.0f * t * (1.0f + t2 * (0.333333333f + t2 * (0.2f + t2 * 0.142857143f)));
}

static float pt_sqrtf(float x)
{
    float y;
    if (x <= 0.0f) return 0.0f;
    y = pt_bits_f(0x1FBD1DF5u + (pt_f_bits(x) >> 1));
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return y;
}

/* ------------------------------------------------------------------ helpers */
static inline float pt_clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

static inline float pt_rand(pt_t *p)                /* white noise -1..1 */
{
    uint32_t x = p->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    p->rng = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}

static inline float pt_hash(int32_t i)              /* a lattice value -1..1 */
{
    uint32_t x = (uint32_t)i * 2654435761u;
    x ^= x >> 15; x *= 2246822519u; x ^= x >> 13;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}

static float pt_noise1(float x)                     /* smooth value noise, -1..1 */
{
    int32_t i = (int32_t)pt_floorf(x);
    float f = x - (float)i, s = f * f * (3.0f - 2.0f * f);
    return pt_hash(i) * (1.0f - s) + pt_hash(i + 1) * s;
}

static void pt_bandpass(float fc, float q, float *b0, float *a1, float *a2)   /* Web Audio's band-pass */
{
    float w0 = 2.0f * PT_PI * fc / PT_SR, al = pt_sinf(w0) / (2.0f * q), a0 = 1.0f + al;
    *b0 = al / a0;
    *a1 = -2.0f * pt_cosf(w0) / a0;
    *a2 = (1.0f - al) / a0;
}

static inline float pt_move(float cur, float tgt, float up, float down)
{
    if (cur < tgt) { cur += up; return cur > tgt ? tgt : cur; }
    cur -= down;
    return cur < tgt ? tgt : cur;
}

/* ------------------------------------------------------------------ glottis */
static void pt_setup_waveform(pt_t *p, float lambda)
{
    float freq = p->old_f * (1.0f - lambda) + p->new_f * lambda;
    float tens = p->old_t * (1.0f - lambda) + p->new_t * lambda;
    float Rd = 3.0f * (1.0f - tens), Ra, Rk, Rg, Ta, Tp, Te, eps, shift, Delta, rhs, lowerI, upperI, omega, s, y, z;
    p->wave_len = 1.0f / freq;
    Rd = pt_clamp(Rd, 0.5f, 2.7f);
    Ra = -0.01f + 0.048f * Rd;
    Rk = 0.224f + 0.118f * Rd;
    Rg = (Rk / 4.0f) * (0.5f + 1.2f * Rk) / (0.11f * Rd - Ra * (0.5f + 1.2f * Rk));
    Ta = Ra;
    Tp = 1.0f / (2.0f * Rg);
    Te = Tp + Tp * Rk;
    eps = 1.0f / Ta;
    shift = pt_expf(-eps * (1.0f - Te));
    Delta = 1.0f - shift;
    rhs = (1.0f / eps) * (shift - 1.0f) + (1.0f - Te) * shift;
    rhs = rhs / Delta;
    lowerI = -(Te - Tp) / 2.0f + rhs;
    upperI = -lowerI;
    omega = PT_PI / Tp;
    s = pt_sinf(omega * Te);
    y = -PT_PI * s * upperI / (Tp * 2.0f);
    z = pt_logf(y);
    p->alpha = z / (Tp / 2.0f - Te);
    p->E0 = -1.0f / (s * pt_expf(p->alpha * Te));
    p->Delta = Delta;
    p->Te = Te;
    p->omega = omega;
    p->epsilon = eps;
    p->shift = shift;
}

static inline float pt_noise_mod(pt_t *p)
{
    float voiced = 0.1f + 0.2f * pt_fmaxf(0.0f, pt_sinf(2.0f * PT_PI * p->t_wave / p->wave_len));
    float ten = p->tenseness, inten = p->intensity;
    return ten * inten * voiced + (1.0f - ten * inten) * 0.3f;
}

static inline float pt_glottis_step(pt_t *p, float lambda, float noise)
{
    float t, out, asp;
    p->t_wave += 1.0f / PT_SR;
    p->total_time += 1.0f / PT_SR;
    if (p->t_wave > p->wave_len) {
        p->t_wave -= p->wave_len;
        pt_setup_waveform(p, lambda);
    }
    t = p->t_wave / p->wave_len;
    if (t > p->Te)
        out = (-pt_expf(-p->epsilon * (t - p->Te)) + p->shift) / p->Delta;
    else
        out = p->E0 * pt_expf(p->alpha * t) * pt_sinf(p->omega * t);
    out *= p->intensity * p->loudness;
    asp = p->intensity * p->asp_k * pt_noise_mod(p) * noise;
    asp *= 0.2f + 0.02f * pt_noise1(p->total_time * 1.99f);
    return out + asp;
}

static void pt_glottis_block(pt_t *p)
{
    float vib, ui_f = p->freq, ten = p->tenseness;
    const float k = (float)PT_BLK / 512.0f;          /* the original moves once per 512 frames */
    vib = p->vib_amount * pt_sinf(2.0f * PT_PI * p->total_time * 6.0f);
    vib += 0.02f * pt_noise1(p->total_time * 4.07f);
    vib += 0.04f * pt_noise1(p->total_time * 2.15f);
    if (ui_f > p->smooth_f) p->smooth_f = pt_fminf(p->smooth_f * (1.0f + 0.1f * 4.0f * k), ui_f);   /* 4 blocks of 1.1 per 512 */
    if (ui_f < p->smooth_f) p->smooth_f = pt_fmaxf(p->smooth_f / (1.0f + 0.1f * 4.0f * k), ui_f);
    p->old_f = p->new_f;
    p->new_f = p->smooth_f * (1.0f + vib);
    p->old_t = p->new_t;
    p->new_t = ten + 0.1f * pt_noise1(p->total_time * 0.46f) + 0.05f * pt_noise1(p->total_time * 0.36f);
    if (!p->gate && p->always_voice) p->new_t += (3.0f - ten) * (1.0f - p->intensity);
    if (p->gate || p->always_voice) p->intensity += 0.13f * k; else p->intensity -= 0.05f * k;
    p->intensity = pt_clamp(p->intensity, 0.0f, 1.0f);
    p->asp_k = 1.0f - pt_sqrtf(ten);
    p->loudness = pt_sqrtf(pt_sqrtf(pt_clamp(ten, 0.02f, 1.0f))) * (0.5f + 0.5f * p->loudness_scale);
}

/* ------------------------------------------------------------------ tract shape */
static void pt_add_transient(pt_t *p, int pos)
{
    pt_trans_t *t;
    if (p->n_trans >= PT_NTRANS) return;
    t = &p->trans[p->n_trans++];
    t->pos = (float)pos; t->alive = 0.0f; t->life = 0.2f; t->strength = 0.3f; t->expo = 200.0f;
}

static void pt_shrink(pt_t *p, float index, float diameter)     /* a constriction, as the original's touch */
{
    float width;
    int ii, a, w2;
    if (index < 25.0f) width = 10.0f;
    else if (index >= (float)PT_TIP) width = 5.0f;
    else width = 10.0f - 5.0f * (index - 25.0f) / (float)(PT_TIP - 25);
    if (index < 2.0f || index >= (float)PT_N || diameter >= 3.0f) return;
    ii = (int)(index + 0.5f);
    w2 = (int)pt_ceilf(width) + 1;
    for (a = -w2; a < (int)width + 1; a++) {
        float rel, shrink;
        int k = ii + a;
        if (k < 0 || k >= PT_N) continue;
        rel = pt_fabsf((float)k - index) - 0.5f;
        if (rel <= 0.0f) shrink = 0.0f;
        else if (rel > width) shrink = 1.0f;
        else shrink = 0.5f * (1.0f - pt_cosf(PT_PI * rel / width));
        if (diameter < p->target[k]) p->target[k] = diameter + (p->target[k] - diameter) * shrink;
    }
}

/* rest shape from the tongue, then the lips and a consonant on top */
static void pt_shape(pt_t *p)
{
    int i;
    float tidx = p->tongue_idx, tdia = p->tongue_dia, fixed;
    p->shape_dirty = 0;
    fixed = 2.0f + (tdia - 2.0f) / 1.5f;
    for (i = PT_BLADE; i < PT_LIP; i++) {
        float t = 1.1f * PT_PI * (tidx - (float)i) / (float)(PT_TIP - PT_BLADE);
        float curve = (1.5f - fixed + 1.7f) * pt_cosf(t);
        if (i == PT_BLADE - 2 || i == PT_LIP - 1) curve *= 0.8f;
        if (i == PT_BLADE || i == PT_LIP - 2) curve *= 0.94f;
        p->rest[i] = 1.5f - curve;
    }
    for (i = 0; i < PT_N; i++) p->target[i] = p->rest[i];
    pt_shrink(p, 41.5f, p->lip_dia);
    if (p->cons_on > 0.5f) pt_shrink(p, p->cons_idx, p->cons_dia);
}

static void pt_calc_reflections(pt_t *p)
{
    int i;
    float sum;
    for (i = 0; i < PT_M; i++) {
        float a = p->diameter[2 * i], b = p->diameter[2 * i + 1];
        p->A[i] = 0.5f * (a * a + b * b);
        if (a <= 0.0f || b <= 0.0f) p->A[i] = 0.0f;   /* a closure at either half closes the pair */
    }
    for (i = 1; i < PT_M; i++) {
        p->refl[i] = p->new_refl[i];
        if (p->A[i] == 0.0f) p->new_refl[i] = 0.999f;
        else p->new_refl[i] = (p->A[i - 1] - p->A[i]) / (p->A[i - 1] + p->A[i]);
    }
    p->refl_left = p->new_refl_left; p->refl_right = p->new_refl_right; p->refl_nose = p->new_refl_nose;
    sum = p->A[PT_NOSE_M] + p->A[PT_NOSE_M + 1] + p->nose_A[0];
    p->new_refl_left = (2.0f * p->A[PT_NOSE_M] - sum) / sum;
    p->new_refl_right = (2.0f * p->A[PT_NOSE_M + 1] - sum) / sum;
    p->new_refl_nose = (2.0f * p->nose_A[0] - sum) / sum;
}

static void pt_reshape(pt_t *p, float dt)
{
    int i, new_last = -1;
    float amount = dt * p->move_speed;
    for (i = 0; i < PT_N; i++) {
        float slow;
        if (p->diameter[i] <= 0.0f) new_last = i;
        if (i < PT_NOSE_START) slow = 0.6f;
        else if (i >= PT_TIP) slow = 1.0f;
        else slow = 0.6f + 0.4f * (float)(i - PT_NOSE_START) / (float)(PT_TIP - PT_NOSE_START);
        p->diameter[i] = pt_move(p->diameter[i], p->target[i], slow * amount, 2.0f * amount);
    }
    if (p->last_obstruction > -1 && new_last == -1 && p->nose_A[0] < 0.05f)
        pt_add_transient(p, p->last_obstruction);
    p->last_obstruction = new_last;
    p->velum_d = pt_move(p->velum_d, p->velum, amount * 0.25f, amount * 0.1f);
    p->nose_A[0] = p->velum_d * p->velum_d;
}

/* ------------------------------------------------------------------ tract step */
static inline void pt_turbulence(pt_t *p, float noise, float index, float diameter)
{
    int i;
    float delta, thin, open, n0, n1;
    index *= 0.5f;                                   /* into the waveguide's segments */
    i = (int)index;
    delta = index - (float)i;
    if (i < 1 || i + 2 >= PT_M) return;              /* (the original's arrays grow; ours end) */
    noise *= pt_noise_mod(p);
    thin = pt_clamp(8.0f * (0.7f - diameter), 0.0f, 1.0f);
    open = pt_clamp(30.0f * (diameter - 0.3f), 0.0f, 1.0f);
    n0 = noise * (1.0f - delta) * thin * open;
    n1 = noise * delta * thin * open;
    p->R[i + 1] += n0 * 0.5f; p->L[i + 1] += n0 * 0.5f;
    p->R[i + 2] += n1 * 0.5f; p->L[i + 2] += n1 * 0.5f;
}

static inline void pt_tract_step(pt_t *p, float glottal, float fnoise)
{
    int i;
    float *R = p->R, *L = p->L, *jR = p->jR, *jL = p->jL, *cr = p->cur_refl;
    /* transients (a plosive's burst) */
    for (i = 0; i < p->n_trans; i++) {
        pt_trans_t *t = &p->trans[i];
        float amp = t->strength * pt_expf(-0.693147f * t->expo * t->alive);
        int at = (int)t->pos >> 1;
        R[at] += amp * 0.5f;
        L[at] += amp * 0.5f;
        t->alive += 1.0f / PT_SR;
        if (t->alive > t->life) { *t = p->trans[--p->n_trans]; i--; }
    }
    if (p->cons_fric > 0.0f)
        pt_turbulence(p, 1.32f * fnoise * p->cons_fric, p->cons_idx, p->cons_dia);

    jR[0] = L[0] * p->glottal_refl + glottal;
    jL[PT_M] = R[PT_M - 1] * p->lip_refl;
    for (i = 1; i < PT_M; i++) {
        float w = cr[i] * (R[i - 1] + L[i]);
        jR[i] = R[i - 1] - w;
        jL[i] = L[i] + w;
    }
    i = PT_NOSE_M;
    jL[i] = p->cur_left * R[i - 1] + (1.0f + p->cur_left) * (p->noseL[0] + L[i]);
    jR[i] = p->cur_right * L[i] + (1.0f + p->cur_right) * (R[i - 1] + p->noseL[0]);
    p->njR[0] = p->cur_nose * p->noseL[0] + (1.0f + p->cur_nose) * (L[i] + R[i - 1]);
    for (i = 0; i < PT_M; i++) {
        R[i] = jR[i] * p->damp;
        L[i] = jL[i + 1] * p->damp;
    }
    p->lip_out = R[PT_M - 1];

    p->njL[PT_NM] = p->noseR[PT_NM - 1] * p->lip_refl;
    for (i = 1; i < PT_NM; i++) {
        float w = p->nose_refl[i] * (p->noseR[i - 1] + p->noseL[i]);
        p->njR[i] = p->noseR[i - 1] - w;
        p->njL[i] = p->noseL[i] + w;
    }
    for (i = 0; i < PT_NM; i++) {
        p->noseR[i] = p->njR[i] * p->fade;
        p->noseL[i] = p->njL[i + 1] * p->fade;
    }
    p->nose_out = p->noseR[PT_NM - 1];
}

/* ------------------------------------------------------------------ init / render */
static void pt_init(pt_t *p)
{
    int i;
    float *z = (float *)p;
    for (i = 0; i < (int)(sizeof *p / sizeof(float)); i++) z[i] = 0.0f;   /* every member is a float or an int: all-zero bits */
    p->rng = 0x12345678u;
    p->freq = p->smooth_f = p->old_f = p->new_f = 140.0f;
    p->tenseness = 0.6f; p->old_t = p->new_t = 0.6f;
    p->loudness_scale = 1.0f; p->loudness = 1.0f;
    p->vib_amount = 0.005f;
    p->velum = 0.01f;
    p->tongue_idx = 12.9f; p->tongue_dia = 2.43f; p->lip_dia = 3.0f;
    p->cons_idx = 38.0f; p->cons_dia = 0.5f;
    p->glottal_refl = 0.75f; p->lip_refl = -0.85f; p->damp = p->fade = 0.998f; p->move_speed = 15.0f;   /* 0.999 per original step: two of them per ours */
    p->last_obstruction = -1;
    for (i = 0; i < PT_N; i++) {
        float d;
        if ((float)i < 7.0f * PT_N / 44.0f - 0.5f) d = 0.6f;
        else if ((float)i < 12.0f * PT_N / 44.0f) d = 1.1f;
        else d = 1.5f;
        p->diameter[i] = p->rest[i] = p->target[i] = d;
    }
    {
        float nd[PT_NOSE_LEN];                       /* the original's nose, then the pairs' means */
        for (i = 0; i < PT_NOSE_LEN; i++) {
            float d = 2.0f * ((float)i / (float)PT_NOSE_LEN);
            d = d < 1.0f ? 0.4f + 1.6f * d : 0.5f + 1.5f * (2.0f - d);
            nd[i] = d > 1.9f ? 1.9f : d;
        }
        for (i = 0; i < PT_NM; i++) {
            float a = nd[2 * i], b = nd[2 * i + 1];
            p->nose_A[i] = 0.5f * (a * a + b * b);
        }
        p->velum_d = 0.01f;
        p->nose_A[0] = p->velum_d * p->velum_d;
        for (i = 1; i < PT_NM; i++)
            p->nose_refl[i] = (p->nose_A[i - 1] - p->nose_A[i]) / (p->nose_A[i - 1] + p->nose_A[i]);
    }
    pt_bandpass(500.0f, 0.5f, &p->asp_b0, &p->asp_a1, &p->asp_a2);
    pt_bandpass(1000.0f, 0.5f, &p->fri_b0, &p->fri_a1, &p->fri_a2);
    pt_shape(p);
    for (i = 0; i < PT_N; i++) p->diameter[i] = p->target[i];
    pt_calc_reflections(p);
    pt_calc_reflections(p);                      /* twice: old and new alike */
    for (i = 0; i < PT_M + 1; i++) p->cur_refl[i] = p->new_refl[i];
    p->cur_left = p->new_refl_left; p->cur_right = p->new_refl_right; p->cur_nose = p->new_refl_nose;
}

/* n mono frames into out[] (overwritten). */
static void pt_render(pt_t *p, float *out, uint32_t n)
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        float lam1, lam2, wn, an, fn, glottal, voc;
        int bp = p->block_pos;
        if (!(bp & (PT_SUB - 1))) {              /* a new step of coefficients */
            float lam = ((float)bp + 0.5f * PT_SUB) / (float)PT_BLK;
            int i;
            for (i = 1; i < PT_M; i++) p->cur_refl[i] = p->refl[i] * (1.0f - lam) + p->new_refl[i] * lam;
            p->cur_left = p->new_refl_left * (1.0f - lam) + p->refl_left * lam;      /* (as the original: new first) */
            p->cur_right = p->new_refl_right * (1.0f - lam) + p->refl_right * lam;
            p->cur_nose = p->new_refl_nose * (1.0f - lam) + p->refl_nose * lam;
            p->cons_fric = pt_clamp(p->cons_fric + (p->cons_on > 0.5f ? 1.0f : -1.0f) * ((float)PT_SUB / PT_SR) / 0.1f, 0.0f, 1.0f);
        }
        lam1 = (float)bp / (float)PT_BLK;
        lam2 = ((float)bp + 0.5f) / (float)PT_BLK;
        (void)lam2;
        wn = pt_rand(p);
        an = p->asp_b0 * wn - p->asp_b0 * p->asp_x2 - p->asp_a1 * p->asp_y1 - p->asp_a2 * p->asp_y2;
        p->asp_x2 = p->asp_x1; p->asp_x1 = wn; p->asp_y2 = p->asp_y1; p->asp_y1 = an;
        wn = pt_rand(p);
        fn = p->fri_b0 * wn - p->fri_b0 * p->fri_x2 - p->fri_a1 * p->fri_y1 - p->fri_a2 * p->fri_y2;
        p->fri_x2 = p->fri_x1; p->fri_x1 = wn; p->fri_y2 = p->fri_y1; p->fri_y1 = fn;
        glottal = pt_glottis_step(p, lam1, an);
        pt_tract_step(p, glottal, fn);
        voc = p->lip_out + p->nose_out;
        out[j] = voc * PT_OUT_GAIN;
        if (++p->block_pos >= PT_BLK) {
            p->block_pos = 0;
            pt_glottis_block(p);
            if (p->shape_dirty) pt_shape(p);
            pt_reshape(p, (float)PT_BLK / PT_SR);
            pt_calc_reflections(p);
        }
    }
}

#endif
