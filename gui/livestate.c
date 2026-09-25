/* gui/livestate.c — live display state from the audio clock (§12.10 G6a). */
#include "gui/livestate.h"

/* 10^(dB/20) for dB = -36 .. 0 (precomputed: no libm in the GUI core). */
static const float RI_LIVE_DB_AMP[37] = {
    0.0158489f, 0.0177828f, 0.0199526f, 0.0223872f, 0.0251189f, 0.0281838f, 0.0316228f,
    0.0354813f, 0.0398107f, 0.0446684f, 0.0501187f, 0.0562341f, 0.0630957f, 0.0707946f,
    0.0794328f, 0.0891251f, 0.1000000f, 0.1122018f, 0.1258925f, 0.1412538f, 0.1584893f,
    0.1778279f, 0.1995262f, 0.2238721f, 0.2511886f, 0.2818383f, 0.3162278f, 0.3548134f,
    0.3981072f, 0.4466836f, 0.5011872f, 0.5623413f, 0.6309573f, 0.7079458f, 0.7943282f,
    0.8912509f, 1.0000000f
};

uint64_t ri_live_16ths(uint64_t samples, uint32_t bpm, uint32_t sr) {
    uint64_t den = 15u * (uint64_t)sr;   /* 60 s / 4 sixteenths per beat */
    if (bpm == 0u || sr == 0u)
        return 0u;
    /* samples * bpm cannot overflow for any real session: 2^64 / 500 is
     * 1.2e16 samples = ~7,600 years at 48 kHz. */
    return samples * (uint64_t)bpm / den;
}

uint32_t ri_live_step(uint64_t sixteenths, uint32_t len) {
    if (len == 0u || len > 16u)
        len = 16u;
    return (uint32_t)(sixteenths % len);
}

uint64_t ri_live_bar(uint64_t start_bar, uint64_t sixteenths) {
    return start_bar + sixteenths / 16u;
}

int ri_live_meter_level(float peak) {
    int k;
    if (!(peak >= RI_LIVE_DB_AMP[0]))   /* also catches NaN */
        return 0;
    for (k = 36; k > 0; k--)
        if (peak >= RI_LIVE_DB_AMP[k])
            break;
    return (k * 127 + 18) / 36;         /* -36..0 dB -> 0..127 */
}

int ri_live_gr_level(float gr_db) {
    float r = -gr_db;                   /* reduction in dB, >= 0 */
    if (!(r > 0.0f))
        return 0;
    if (r >= (float)RI_LIVE_GR_FULL_DB)
        return 127;
    return (int)(r * 127.0f / (float)RI_LIVE_GR_FULL_DB + 0.5f);
}
