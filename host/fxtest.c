/* Host test of pt_fx.h: an impulse train in, each effect at full, then silence: the tails must die and nothing may blow up. */
#include <stdio.h>
#include <math.h>
#include "../pt_fx.h"
static pt_fx_t F;
int main(void)
{
    pt_fx_init(&F);
    F.chorus = 1; F.delay_mix = 1; F.reverb_mix = 1;
    float peak = 0, tail = 0;
    for (int n = 0; n < 44100 * 8; n++) {
        float x = n < 22050 ? 0.5f * sinf(n * 0.05f) : 0.0f, l, r;
        pt_fx_step(&F, x, &l, &r);
        if (fabsf(l) > peak) peak = fabsf(l);
        if (n > 44100 * 7) tail += fabsf(l) + fabsf(r);
        if (l != l || r != r) { printf("NaN at %d\n", n); return 1; }
    }
    printf("peak %.3f, tail energy after 7 s %.6f, sizeof %zu\n", peak, tail, sizeof F);
    return tail > 0.5 ? 1 : 0;
}
