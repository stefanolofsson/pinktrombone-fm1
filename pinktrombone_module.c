/* SPDX-License-Identifier: GPL-3.0-only */
/* PINK TROMBONE for the M-VAVE FM-1: a fm1-dock module (and, built solo, a firmware of its own).
 *
 * The voice is Neil Thapen's Pink Trombone (MIT): a glottis and a 44-segment vocal tract with a nose, pt_engine.h.
 * One voice, monophonic, last note first.
 *
 *   keys / MIDI    the pitch (the keys' F3 is a bass F2, as the original's keyboard; OCT+ goes up)
 *   KNOB1          tongue position   (back .. front)
 *   KNOB2          tongue height     (high .. low)
 *   KNOB3          lips              (rounded .. open)
 *   KNOB4          tenseness         (a breath .. a pressed voice)
 *   PRESETS        a vowel: AH AE EH IH EE UH AW OH OO
 *   SELECT         vibrato          ALGORITHM   the nose (the nasal sound)
 *   hold SEQ = S, SEL = SH, ENV = F, LFO = T / D, EDIT = K / G, GLO = P / B, ARP = M / N (a nasal)
 *   FX             the effects page (toggle): KNOB1 chorus, KNOB2 delay, KNOB3 reverb, KNOB4 the delay's time
 *   PLAY           the voice drones (toggle)       SAVE   keeps the settings
 * MIDI CC: 1 vibrato, 74 tongue position, 71 tongue height, 72 lips, 73 tenseness, 76 nose; 123 all notes off,
 * 91 reverb, 92 delay, 93 chorus. */
#include "hb_module.h"
#include "pt_engine.h"
#include "pt_gfx.h"
#include "pt_fx.h"

static const hb_host_t *H;
static pt_t P;

/* ---- the controls the panel shows ---- */
static struct {
    uint32_t magic;
    float tidx, tdia, lip, tens, vib, nose;
    uint32_t vowel;
    float chorus, delay, reverb, dtime;         /* the effects: 0..1, the delay's time in ms */
} S;
#define S_MAGIC 0x50544D32u                     /* "PTM2" */
static pt_fx_t FX;
static uint32_t fx_page;                        /* the effects page: the knobs are the effects' */

typedef struct { const char *name; float idx, dia, lip; } vowel_t;
static const vowel_t VOWEL[] = {                /* tuned on this computer against male formants (host/tune_vowels.py) */
    {"AH", 12.0f, 2.30f, 3.0f}, {"AE", 22.0f, 3.40f, 3.0f}, {"EH", 24.0f, 2.70f, 3.0f},
    {"IH", 26.0f, 2.40f, 3.0f}, {"EE", 27.0f, 2.10f, 3.0f}, {"UH", 12.9f, 2.43f, 3.0f},
    {"AW", 15.0f, 2.05f, 1.3f}, {"OH", 18.0f, 2.30f, 0.6f}, {"OO", 14.0f, 2.05f, 0.5f},
};
#define NVOWEL (sizeof VOWEL / sizeof VOWEL[0])

typedef struct { uint8_t btn; const char *name; float idx, dia; } cons_t;
static const cons_t CONS[] = {
    {HB_BTN_SEQ, "S",   38.5f, 0.45f}, {HB_BTN_SEL, "SH",  34.0f, 0.50f}, {HB_BTN_ENV, "F",   42.5f, 0.50f},
    {HB_BTN_LFO, "T D", 37.0f, 0.00f}, {HB_BTN_EDIT, "K G", 25.0f, 0.00f}, {HB_BTN_GLO, "P B", 43.0f, 0.00f},
};
#define NCONS (sizeof CONS / sizeof CONS[0])

static uint8_t held[8];                         /* keys down, in order */
static uint32_t n_held, cons_held, nasal_held, drone;
static int32_t cur_note = -1;
static uint32_t drawn_sig, last_draw_ms, force_draw;

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

static void apply(void)                         /* the settings to the voice */
{
    P.tongue_idx = S.tidx;
    P.tongue_dia = S.tdia;
    P.lip_dia = S.lip;
    P.tenseness = S.tens;
    P.vib_amount = S.vib;
    P.velum = nasal_held ? 0.4f : S.nose;
    P.shape_dirty = 1;
    FX.chorus = S.chorus; FX.delay_mix = S.delay; FX.reverb_mix = S.reverb; FX.delay_ms = S.dtime;
    force_draw = 1;
}

static void set_vowel(uint32_t v)
{
    S.vowel = v;
    S.tidx = VOWEL[v].idx;
    S.tdia = VOWEL[v].dia;
    S.lip = VOWEL[v].lip;
    apply();
}

static void defaults(void)
{
    S.magic = S_MAGIC;
    S.tens = 0.6f; S.vib = 0.005f; S.nose = 0.01f;
    S.chorus = 0.3f; S.delay = 0.0f; S.reverb = 0.25f; S.dtime = 280.0f;
    set_vowel(5);                               /* UH: the original's neutral tract */
}

static void pitch(uint32_t key)
{
    P.freq = 440.0f * pt_expf(((float)key - 12.0f - 69.0f) * 0.0577622650f);
    cur_note = (int32_t)key;
}

/* ---- audio ---- */
#define RING_N 1024u
static int16_t ring[RING_N];                    /* the last output, for the scope */
static volatile uint32_t ring_w;
static volatile float peak_acc;                 /* the loudest sample since the picture last looked */
static uint16_t FB[GW * GH];                    /* the picture */
#define GAIN 2.6f
static void render(int32_t *out, uint32_t n)
{
    float buf[128];
    uint32_t i, k, m;
    for (k = 0; k < n; k += m) {
        m = n - k > 128u ? 128u : n - k;
        pt_render(&P, buf, m);
        for (i = 0; i < m; i++) {
            float x = buf[i] * GAIN, l, r;
            int32_t s;
            x = x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x;
            s = (int32_t)(x * 30000.0f);
            ring[ring_w++ & (RING_N - 1u)] = (int16_t)s;
            if (x > peak_acc) peak_acc = x;
            if (-x > peak_acc) peak_acc = -x;
            pt_fx_step(&FX, x, &l, &r);
            l = l > 1.0f ? 1.0f : l < -1.0f ? -1.0f : l;
            r = r > 1.0f ? 1.0f : r < -1.0f ? -1.0f : r;
            out[2u * (k + i)] += (int32_t)(l * 30000.0f);
            out[2u * (k + i) + 1u] += (int32_t)(r * 30000.0f);
        }
    }
}

/* ---- notes ---- */
static void note(uint32_t ch, uint32_t key, uint32_t vel)
{
    uint32_t i;
    (void)ch;
    if (key > 127u)
        return;
    for (i = 0; i < n_held; i++)                /* out of the stack (a repeat goes to the top) */
        if (held[i] == key) {
            for (; i + 1u < n_held; i++) held[i] = held[i + 1u];
            n_held--;
            break;
        }
    if (vel) {
        if (n_held == sizeof held) {
            for (i = 0; i + 1u < n_held; i++) held[i] = held[i + 1u];
            n_held--;
        }
        held[n_held++] = (uint8_t)key;
        P.loudness_scale = (float)vel / 127.0f;
        pitch(key);
        P.gate = 1;
    } else if (n_held) {
        pitch(held[n_held - 1u]);
    } else {
        P.gate = 0;
    }
    force_draw = 1;
}

static void all_off(void)
{
    n_held = 0;
    P.gate = 0;
    cons_held = nasal_held = 0;
    P.cons_on = 0.0f;
    apply();
}

static void cc(uint32_t ch, uint32_t num, uint32_t val)
{
    float v = (float)val / 127.0f;
    (void)ch;
    switch (num) {
    case 1:  S.vib = v * 0.03f; break;
    case 74: S.tidx = 12.0f + v * 17.0f; break;
    case 71: S.tdia = 2.05f + v * 1.45f; break;
    case 72: S.lip = 0.5f + v * 2.5f; break;
    case 73: S.tens = v; break;
    case 76: S.nose = 0.01f + v * 0.39f; break;
    case 91: S.reverb = v; break;
    case 92: S.delay = v; break;
    case 93: S.chorus = v; break;
    case 123: all_off(); return;
    default: return;
    }
    apply();
}

/* ---- panel ---- */
static void cons_update(void)
{
    uint32_t i;
    int pick = -1;
    for (i = 0; i < NCONS; i++)                 /* the last one pressed that is still down: the bit order is the press order's stand-in */
        if (cons_held >> i & 1u) pick = (int)i;
    if (pick < 0) {
        P.cons_on = 0.0f;
    } else {
        P.cons_idx = CONS[pick].idx;
        P.cons_dia = CONS[pick].dia;
        P.cons_on = 1.0f;
    }
    P.shape_dirty = 1;
    for (i = 0; i < NCONS; i++)
        H->led(CONS[i].btn, cons_held >> i & 1u);
    force_draw = 1;
}

static void ui(uint32_t ev, int32_t arg)
{
    uint32_t kind = HB_EV_KIND(ev), id = HB_EV_ID(ev), i;
    if (kind == HB_EV_ENC) {
        float c = (float)arg;
        if (fx_page && id >= HB_ENC_KNOB1 && id <= HB_ENC_KNOB4) {     /* the effects page */
            switch (id) {
            case HB_ENC_KNOB1: S.chorus = clampf(S.chorus + c * 0.02f, 0.0f, 1.0f); break;
            case HB_ENC_KNOB2: S.delay = clampf(S.delay + c * 0.02f, 0.0f, 1.0f); break;
            case HB_ENC_KNOB3: S.reverb = clampf(S.reverb + c * 0.02f, 0.0f, 1.0f); break;
            default: S.dtime = clampf(S.dtime + c * 5.0f, 60.0f, 360.0f); break;
            }
            apply();
            return;
        }
        switch (id) {
        case HB_ENC_KNOB1: S.tidx = clampf(S.tidx + c * 0.25f, 12.0f, 29.0f); break;
        case HB_ENC_KNOB2: S.tdia = clampf(S.tdia + c * 0.03f, 2.05f, 3.5f); break;
        case HB_ENC_KNOB3: S.lip = clampf(S.lip + c * 0.1f, 0.5f, 3.0f); break;
        case HB_ENC_KNOB4: S.tens = clampf(S.tens + c * 0.02f, 0.0f, 1.0f); break;
        case HB_ENC_SELECT: S.vib = clampf(S.vib + c * 0.001f, 0.0f, 0.03f); break;
        case HB_ENC_ALGORITHM: S.nose = clampf(S.nose + c * 0.02f, 0.01f, 0.4f); break;
        case HB_ENC_PRESETS: {
            int32_t v = (int32_t)S.vowel + (arg > 0 ? 1 : -1);
            set_vowel((uint32_t)(v < 0 ? (int32_t)NVOWEL - 1 : v >= (int32_t)NVOWEL ? 0 : v));
            return;
        }
        default: return;
        }
        apply();
        return;
    }
    if (kind == HB_EV_BTN_DOWN || kind == HB_EV_BTN_UP) {
        uint32_t down = kind == HB_EV_BTN_DOWN;
        for (i = 0; i < NCONS; i++)
            if (CONS[i].btn == id) {
                if (down) cons_held |= 1u << i; else cons_held &= ~(1u << i);
                cons_update();
                return;
            }
        if (id == HB_BTN_FX) {
            if (down) {
                fx_page = !fx_page;
                H->led(HB_BTN_FX, fx_page);
                force_draw = 1;
            }
        } else if (id == HB_BTN_ARP) {
            nasal_held = down;
            H->led(HB_BTN_ARP, down);
            apply();
        } else if (id == HB_BTN_PLAY && down) {
            drone = !drone;
            P.always_voice = (int)drone;
            H->led(HB_BTN_PLAY, drone);
        } else if (id == HB_BTN_SAVE && down) {
            H->save(&S, sizeof S);
            H->log("pinktrombone: saved");
        }
    }
}

/* ---- screen ---- */
static void fmt_f(char *s, float v, uint32_t dec)   /* v >= 0 */
{
    uint32_t whole, frac, k, scale = 1;
    for (k = 0; k < dec; k++) scale *= 10u;
    v = v < 0.0f ? 0.0f : v;
    whole = (uint32_t)v;
    frac = (uint32_t)((v - (float)whole) * (float)scale + 0.5f);
    if (frac >= scale) { whole++; frac -= scale; }
    if (whole >= 100u) *s++ = (char)('0' + whole / 100u % 10u);
    if (whole >= 10u) *s++ = (char)('0' + whole / 10u % 10u);
    *s++ = (char)('0' + whole % 10u);
    if (dec) {
        *s++ = '.';
        for (k = dec; k > 0; k--) { scale /= 10u; *s++ = (char)('0' + frac / scale % 10u); }
    }
    *s = 0;
}

static void cat(char *d, const char *s)
{
    while (*d) d++;
    while ((*d++ = *s++)) {}
}

static const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

#define PINK  HB_RGB(255, 105, 180)
#define DARK  HB_RGB(86, 24, 52)
#define GREY  HB_RGB(120, 120, 120)

#define GFX_Y 62
static float level_disp;

static void draw_tract(void)
{
    float pk = peak_acc;
    peak_acc = 0.0f;
    level_disp = pk > level_disp ? pk : level_disp * 0.82f;         /* the glow follows the voice, falling off quickly */
    pt_gfx_draw(FB, &P, level_disp * 1.4f, ring, RING_N, ring_w);
    H->blit(0, GFX_Y, GW, GH, FB);
}

static void draw(uint32_t full)
{
    char b[40], t[8];
    uint32_t sig, now = H->ms(), i;
    int active;
    if (full) {
        H->fill(0, HB_TOP, 240, 240 - HB_TOP, 0);
        force_draw = 1;
        drawn_sig = ~0u;
    }
    /* the picture moves while the voice sounds or the tract changes shape: ~20 fps is plenty, and nothing when still */
    sig = 0;
    for (i = 0; i < PT_N; i += 4) sig = sig * 31u + (uint32_t)(P.diameter[i] * 40.0f);
    sig = sig * 31u + (uint32_t)(P.velum_d * 100.0f) + (P.cons_on > 0.5f ? 7u : 0u) + (uint32_t)(P.cons_idx);
    active = P.intensity > 0.01f || level_disp > 0.01f || peak_acc > 0.0f;
    if ((sig != drawn_sig || force_draw || active) && (now - last_draw_ms >= 45u || full)) {
        draw_tract();
        drawn_sig = sig;
        last_draw_ms = now;
    }
    if (force_draw) {
        const char *v = "--";
        uint32_t k;
        force_draw = 0;
        for (k = 0; k < NVOWEL; k++)
            if (pt_fabsf(S.tidx - VOWEL[k].idx) < 0.01f && pt_fabsf(S.tdia - VOWEL[k].dia) < 0.01f &&
                pt_fabsf(S.lip - VOWEL[k].lip) < 0.01f)
                v = VOWEL[k].name;
        H->fill(0, 26, 240, GFX_Y - 26, 0);
        H->text(0, 27, 240, 1, v, PINK, 1);
        if (cons_held) {
            for (k = 0; k < NCONS; k++)
                if (cons_held >> k & 1u) v = CONS[k].name;
            H->text(176, 34, 64, 0, v, 0xFFFFu, 0);
        }
        if (cur_note >= 12) {                    /* the pitch that sounds: an octave under the key */
            int32_t sn = cur_note - 12;
            char o[4];
            b[0] = 0;
            cat(b, NOTE[sn % 12]);
            o[0] = (char)('0' + (sn / 12 - 1 + 10) % 10); o[1] = 0;
            cat(b, o);
            H->text(8, 30, 70, 0, b, GREY, 0);
        }
        if (drone) H->text(8, 46, 70, 0, "DRONE", PINK, 0);
        H->fill(0, GFX_Y + GH, 240, 240 - GFX_Y - GH, 0);
        if (fx_page) {
            b[0] = 0; cat(b, "CHORUS "); fmt_f(t, S.chorus * 100.0f, 0); cat(b, t); cat(b, "  DELAY "); fmt_f(t, S.delay * 100.0f, 0); cat(b, t);
            H->text(0, GFX_Y + GH + 4, 240, 0, b, HB_RGB(150, 230, 255), 1);
            b[0] = 0; cat(b, "REVERB "); fmt_f(t, S.reverb * 100.0f, 0); cat(b, t); cat(b, "  TIME "); fmt_f(t, S.dtime, 0); cat(b, t); cat(b, " MS");
            H->text(0, GFX_Y + GH + 22, 240, 0, b, HB_RGB(150, 230, 255), 1);
            H->text(176, 46, 64, 0, "FX", HB_RGB(150, 230, 255), 0);
            return;
        }
        b[0] = 0; cat(b, "TNG "); fmt_f(t, S.tidx, 1); cat(b, t); cat(b, "  HGT "); fmt_f(t, S.tdia, 2); cat(b, t);
        cat(b, "  LIP "); fmt_f(t, S.lip, 1); cat(b, t);
        H->text(0, GFX_Y + GH + 4, 240, 0, b, HB_RGB(255, 190, 220), 1);
        b[0] = 0; cat(b, "TEN "); fmt_f(t, S.tens, 2); cat(b, t); cat(b, "  NOSE "); fmt_f(t, (S.nose - 0.01f) / 0.39f * 100.0f, 0); cat(b, t);
        cat(b, "%  VIB "); fmt_f(t, S.vib / 0.03f * 100.0f, 0); cat(b, t); cat(b, "%");
        H->text(0, GFX_Y + GH + 22, 240, 0, b, HB_RGB(255, 190, 220), 1);
    }
}

static int init(const hb_host_t *h)
{
    H = h;
    pt_init(&P);
    pt_fx_init(&FX);
    defaults();
    if (H->load(&S, sizeof S) || S.magic != S_MAGIC || S.tidx < 11.9f || S.tidx > 29.1f || S.vowel >= NVOWEL || S.dtime < 59.0f || S.dtime > 361.0f)
        defaults();
    else
        apply();
    P.always_voice = 0;
    drone = 0;
    H->log("pinktrombone: ready");
    return 0;
}

static void release(void)
{
    fx_page = 0;
    all_off();
}

HB_DESC = {HB_ABI, "PINK TROMBONE", init, release, note, cc, render, ui, draw, 0, 0, 0, 0};
