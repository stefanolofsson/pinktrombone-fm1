/* Host test: render the voice with given controls to a raw float file.
 *   render out.raw seconds freq tongue_idx tongue_dia lip_dia tenseness velum [cons_idx cons_dia] */
#include <stdio.h>
#include <stdlib.h>
#include "../pt_engine.h"
int main(int argc, char **argv)
{
    static pt_t p;
    uint32_t n = (uint32_t)(atof(argv[2]) * PT_SR), done = 0;
    float buf[256];
    FILE *f = fopen(argv[1], "wb");
    pt_init(&p);
    p.freq = (float)atof(argv[3]);
    p.tongue_idx = (float)atof(argv[4]); p.tongue_dia = (float)atof(argv[5]); p.lip_dia = (float)atof(argv[6]);
    p.tenseness = (float)atof(argv[7]); p.velum = (float)atof(argv[8]);
    if (argc > 10) { p.cons_idx = (float)atof(argv[9]); p.cons_dia = (float)atof(argv[10]); p.cons_on = 1; }
    p.shape_dirty = 1; p.gate = 1;
    while (done < n) { pt_render(&p, buf, 256); fwrite(buf, 4, 256, f); done += 256; }
    fclose(f);
    return 0;
}
