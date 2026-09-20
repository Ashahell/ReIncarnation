/* mixer.c — 4 section buses + master + meter tap + mono send bus
 * (Task 11, gate G11). No allocation; no IO; bounded loops; ri_* only.
 */
#include "engine/mixer/mixer.h"
#include "engine/dsp/kernels.h"

float ri_fader_gain(uint8_t v) {
    float t = (float)v / 127.0f;
    return t * t;
}

void ri_meter_init(struct RiMeter *t, float sr) {
    if (!t)
        return;
    if (sr <= 0.0f)
        sr = RI_MIX_SR_DEFAULT;
    /* Per-sample hold multiplier for 20 dB/s:
     * coef = 10^(-20/(20*sr)) = 2^(-log2(10)/sr). */
    t->coef = ri_pow2(-RI_MIX_LOG2_10 / sr);
    t->peak = 0.0f;
}

void ri_meter_feed(struct RiMeter *t, const float *b, uint32_t n) {
    uint32_t i;
    if (!t || !b)
        return;
    for (i = 0; i < n; i++) {
        float a = b[i] < 0.0f ? -b[i] : b[i];
        t->peak *= t->coef;
        if (a > t->peak)
            t->peak = a;
        if (t->peak < 1e-30f)
            t->peak = 0.0f; /* denormal snap (pcf.c precedent) */
    }
}

float ri_meter_peak(const struct RiMeter *t) {
    if (!t)
        return 0.0f;
    return t->peak;
}

void ri_mix_init(struct RiMixer *m, float sr) {
    uint32_t b;
    if (!m)
        return;
    if (sr <= 0.0f)
        sr = RI_MIX_SR_DEFAULT;
    m->sr = sr;
    for (b = 0; b < RI_MIX_NBUS; b++) {
        m->bus[b].fader = 127u;
        m->bus[b].send = 0u;
        m->bus[b].mute = 0u;
        m->bus[b].solo = 0u;
        m->bus[b].applied = 0.0f; /* slew state starts silent, settles up */
    }
    m->master = 127u;
    m->master_applied = 0.0f;
    ri_meter_init(&m->meter, sr);
}

int ri_mix_set_fader(struct RiMixer *m, uint32_t bus, uint8_t v) {
    if (!m || bus >= RI_MIX_NBUS)
        return 2;
    m->bus[bus].fader = v;
    return 0;
}

int ri_mix_set_send(struct RiMixer *m, uint32_t bus, uint8_t v) {
    if (!m || bus >= RI_MIX_NBUS)
        return 2;
    m->bus[bus].send = v;
    return 0;
}

int ri_mix_set_mute(struct RiMixer *m, uint32_t bus, uint8_t v) {
    if (!m || bus >= RI_MIX_NBUS)
        return 2;
    m->bus[bus].mute = v ? 1u : 0u;
    return 0;
}

int ri_mix_set_solo(struct RiMixer *m, uint32_t bus, uint8_t v) {
    if (!m || bus >= RI_MIX_NBUS)
        return 2;
    m->bus[bus].solo = v ? 1u : 0u;
    return 0;
}

int ri_mix_set_master(struct RiMixer *m, uint8_t v) {
    if (!m)
        return 2;
    m->master = v;
    return 0;
}

int ri_mix_audible(const struct RiMixer *m, uint32_t bus) {
    uint32_t b;
    if (!m || bus >= RI_MIX_NBUS)
        return 0;
    for (b = 0; b < RI_MIX_NBUS; b++) {
        if (m->bus[b].solo)
            return (m->bus[bus].solo && !m->bus[bus].mute) ? 1 : 0;
    }
    return m->bus[bus].mute ? 0 : 1;
}

/* One slew step toward a target (full 0..1 traverse in RI_MIX_RAMP_SMP). */
static float slew_step(float cur, float tgt) {
    float d = tgt - cur;
    float step = 1.0f / (float)RI_MIX_RAMP_SMP;
    if (d > step)
        d = step;
    else if (d < -step)
        d = -step;
    return cur + d;
}

void ri_mix_render(struct RiMixer *m, const float *bus_in[RI_MIX_NBUS],
    float *out, float *send_out, uint32_t n) {
    uint32_t i, b;
    float mtgt;
    if (!m || !bus_in || !out || !send_out)
        return;
    mtgt = ri_fader_gain(m->master);
    for (i = 0; i < n; i++) {
        float acc = 0.0f, snd = 0.0f;
        for (b = 0; b < RI_MIX_NBUS; b++) {
            float tgt = ri_mix_audible(m, b) ?
                ri_fader_gain(m->bus[b].fader) : 0.0f;
            float cur;
            m->bus[b].applied = slew_step(m->bus[b].applied, tgt);
            cur = m->bus[b].applied;
            if (bus_in[b]) {
                acc += bus_in[b][i] * cur;
                snd += bus_in[b][i] * cur *
                    ri_fader_gain(m->bus[b].send);
            }
        }
        m->master_applied = slew_step(m->master_applied, mtgt);
        out[i] = acc * m->master_applied;
        send_out[i] = snd;
    }
    ri_meter_feed(&m->meter, out, n);
}
