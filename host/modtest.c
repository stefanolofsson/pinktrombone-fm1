/* Host test of the module itself with a stub host: plays a phrase with vowels and consonants into build/host/phrase.wav.
 *   cc -O2 -DHB_DESC='const hb_module_t hb_module' -I../fm1-dock/firmware/dock ... (see host/modtest.sh) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pinktrombone_module.c"

static uint32_t t_ms;
static uint32_t h_ms(void) { return t_ms; }
static uint32_t h_ticks(void) { return 0; }
static void h_log(const char *s) { printf("log: %s\n", s); }
static void h_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
static void h_text(int32_t x, int32_t y, int32_t w, uint32_t big, const char *s, uint16_t fg, uint32_t center) { (void)x;(void)w;(void)fg;(void)center; printf("  text y=%d big=%u '%s'\n", (int)y, big, s); }
static uint16_t cv[240 * 96];
static int32_t cv_w, cv_h;
static void h_cvb(int32_t w, int32_t h, uint16_t bg) { cv_w = w; cv_h = h; for (int i = 0; i < w * h; i++) cv[i] = bg; }
static void h_cvr(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (i >= 0 && i < cv_w && j >= 0 && j < cv_h) cv[j * cv_w + i] = c; }
static void h_cvl(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int n = abs(x1 - x0) > abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0);
    for (int k = 0; k <= n; k++) { int x = n ? x0 + (x1 - x0) * k / n : x0, y = n ? y0 + (y1 - y0) * k / n : y0; if (x >= 0 && x < cv_w && y >= 0 && y < cv_h) cv[y * cv_w + x] = c; }
}
static void h_cvp(int32_t x, int32_t y, uint16_t c) { h_cvl(x, y, x, y, c); }
static void h_cvt(int32_t x, int32_t y, uint32_t big, const char *s, uint16_t fg) { (void)x;(void)y;(void)big;(void)s;(void)fg; }
static void h_cvblit(int32_t x, int32_t y)
{
    FILE *f = fopen("build/host/tract.ppm", "wb");
    (void)x; (void)y;
    fprintf(f, "P6\n%d %d\n255\n", cv_w, cv_h);
    for (int i = 0; i < cv_w * cv_h; i++) { uint16_t c = cv[i]; fputc((c >> 11) << 3, f); fputc(((c >> 5) & 63) << 2, f); fputc((c & 31) << 3, f); }
    fclose(f);
}
static int snap_n;
static void h_blit(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *px)
{
    char name[64];
    FILE *f;
    (void)x; (void)y;
    snprintf(name, sizeof name, "build/host/gfx_%d.ppm", snap_n);
    f = fopen(name, "wb");
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) { uint16_t c = (uint16_t)((px[i] >> 8) | (px[i] << 8)); fputc((c >> 11) << 3, f); fputc(((c >> 5) & 63) << 2, f); fputc((c & 31) << 3, f); }
    fclose(f);
}
static void h_led(uint32_t id, uint32_t on) { (void)id; (void)on; }
static uint32_t h_rand(void) { return (uint32_t)rand(); }
static int h_load(void *p, uint32_t n) { (void)p; (void)n; return 1; }
static void h_save(const void *p, uint32_t n) { (void)p; (void)n; }
static void h_midi(uint32_t a, uint32_t b, uint32_t c) { (void)a;(void)b;(void)c; }
static uint32_t h_master(void) { return 4096; }

static int16_t *pcm; static size_t npcm;
static void run(double sec)
{
    uint32_t frames = (uint32_t)(sec * 44100);
    static int32_t blk[512];
    for (uint32_t d = 0; d < frames; d += 256) {
        memset(blk, 0, sizeof blk);
        hb_module.render(blk, 256);
        pcm = realloc(pcm, (npcm + 256) * 2);
        for (int i = 0; i < 256; i++) pcm[npcm++] = (int16_t)blk[2 * i];
        t_ms += 256 * 1000 / 44100;
        hb_module.draw(0);
    }
}
static void vowel(uint32_t v) { hb_module.ui(HB_EV_ENC | HB_ENC_PRESETS, v ? 1 : -1); }

int main(int argc, char **argv)
{
    hb_host_t h;
    memset(&h, 0, sizeof h);
    h.abi = HB_ABI; h.size = sizeof h; h.ms = h_ms; h.ticks = h_ticks; h.log = h_log; h.fill = h_fill; h.text = h_text;
    h.cv_begin = h_cvb; h.cv_rect = h_cvr; h.cv_line = h_cvl; h.cv_pixel = h_cvp; h.cv_text = h_cvt; h.cv_blit = h_cvblit;
    h.blit = h_blit; h.led = h_led; h.rand = h_rand; h.load = h_load; h.save = h_save; h.midi_send = h_midi; h.master = h_master;
    hb_module.init(&h);
    hb_module.draw(1);
    if (argc > 1) {                                  /* a gallery of the picture: build/host/gfx_N.ppm */
        struct { int presets; int note; int btn; } st[] = {{0, 0, -1}, {4, 62, -1}, {-3, 58, -1}, {0, 55, HB_BTN_FX}, {2, 60, HB_BTN_ARP}, {3, 64, HB_BTN_GLO}};
        int cur = 5;
        for (int n = 0; n < 6; n++) {
            for (int k = 0; k < abs(st[n].presets); k++) hb_module.ui(HB_EV_ENC | HB_ENC_PRESETS, st[n].presets > 0 ? 1 : -1);
            cur += st[n].presets;
            if (st[n].note) hb_module.note(0, st[n].note, 110);
            if (st[n].btn >= 0) hb_module.ui(HB_EV_BTN_DOWN | st[n].btn, 0);
            run(0.7);
            snap_n = n;
            hb_module.draw(1);
            if (st[n].btn >= 0) hb_module.ui(HB_EV_BTN_UP | st[n].btn, 0);
            if (st[n].note) hb_module.note(0, st[n].note, 0);
            run(0.3);
        }
        (void)cur;
        return 0;
    }
    hb_module.cc(0, 92, 40);                         /* some delay too */
    /* "ah-ee-oo" on a melody, then a consonant phrase */
    int notes[] = {50, 53, 55, 58, 55, 53, 50, 46};
    for (int k = 0; k < 8; k++) {
        hb_module.note(0, notes[k], 100);
        vowel(k & 1);
        run(0.28);
        if (k == 3) { hb_module.ui(HB_EV_BTN_DOWN | HB_BTN_FX, 0); run(0.18); hb_module.ui(HB_EV_BTN_UP | HB_BTN_FX, 0); }
        if (k == 5) { hb_module.ui(HB_EV_BTN_DOWN | HB_BTN_LFO, 0); run(0.08); hb_module.ui(HB_EV_BTN_UP | HB_BTN_LFO, 0); }
        if (k == 6) { hb_module.ui(HB_EV_BTN_DOWN | HB_BTN_ARP, 0); run(0.25); hb_module.ui(HB_EV_BTN_UP | HB_BTN_ARP, 0); }
        hb_module.note(0, notes[k], 0);
        run(0.08);
    }
    FILE *f = fopen("build/host/phrase.wav", "wb");
    uint32_t dl = (uint32_t)npcm * 2, v;
    fwrite("RIFF", 1, 4, f); v = 36 + dl; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); uint16_t s = 1; fwrite(&s, 2, 1, f); fwrite(&s, 2, 1, f);
    v = 44100; fwrite(&v, 4, 1, f); v = 88200; fwrite(&v, 4, 1, f); s = 2; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dl, 4, 1, f); fwrite(pcm, 2, npcm, f); fclose(f);
    int peak = 0; for (size_t i = 0; i < npcm; i++) { int a = abs(pcm[i]); if (a > peak) peak = a; }
    printf("%zu frames, peak %d of 32767\n", npcm, peak);
    return 0;
}
