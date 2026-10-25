#include <stdio.h>
#include <math.h>
#include "engine/dsp/levi.h"
#define SR 48000.0f
#define BS 256u
static struct RILeviSet A, B;
static void render(struct RILeviSet *s, int blocks, double *peak, double *rms) {
    static float l[BS], r[BS];
    double p = 0.0, sum = 0.0; long n = 0;
    int i, k;
    for (i = 0; i < blocks; i++) {
        levi_voice_render_sum_stereo(s, l, r, BS, SR);
        for (k = 0; k < (int)BS; k++) {
            double a = l[k] > 0 ? l[k] : -l[k];
            double b = r[k] > 0 ? r[k] : -r[k];
            if (a > p) p = a;
            if (b > p) p = b;
            sum += (double)l[k]*l[k] + (double)r[k]*r[k];
            n += 2;
        }
    }
    *peak = p; *rms = sqrt(sum / (double)n);
}
int main(void) {
    double p1, r1, p2, r2, p3, r3;
    int i;
    levi_init_set(&A); levi_init_set(&B);
    /* live keybed path: velocity 127 */
    levi_note_vel(&A, 60, 127);
    render(&A, 40, &p1, &r1);
    /* song path: levi_trigger on lane 0, note 60 */
    levi_trigger(&B, 0, 60);
    render(&B, 40, &p2, &r2);
    /* song chord: pattern 1 row 4 notes 46,73,34,61 on lanes 0..3 */
    {
        struct RILeviSet C; levi_init_set(&C);
        levi_trigger(&C, 0, 46); levi_trigger(&C, 1, 73);
        levi_trigger(&C, 2, 34); levi_trigger(&C, 3, 61);
        render(&C, 40, &p3, &r3);
    }
    printf("live  127     peak %.9f rms %.9f\n", p1, r1);
    printf("song  1 note  peak %.9f rms %.9f\n", p2, r2);
    printf("song  4 chord peak %.9f rms %.9f\n", p3, r3);
    printf("ratio live/song-note peak %.6f rms %.6f\n", p2>0?p1/p2:0, r2>0?r1/r2:0);
    printf("ratio live/song-chord peak %.6f rms %.6f\n", p3>0?p1/p3:0, r3>0?r1/r3:0);
    printf("pvel A %u B %u\n", A.pvel, B.pvel);
    (void)i;
    return 0;
}
