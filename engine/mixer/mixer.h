/* mixer.h — 4 section buses + master + meter tap + mono send bus
 * (Task 11, gate G11). Spec §13 mixer + Appendix A P-16 (meter, ~20 dB/s
 * peak-hold) + P-17 (fader law) + §6 device order (mixer sums section
 * buses after the barrier; single-threaded Classic).
 *
 * E0 decisions (ledger docs/evidence/sequencer/fader-law.md; P-17 locks
 * at TC-2.6.1): one square law gain = (v/127)^2 for bus faders, master,
 * and sends (post-fader sends into a mono send bus). Mute/solo rule: no
 * solo active -> bus audible iff !mute; any solo active -> bus audible
 * iff solo && !mute. Toggles are zipless by construction: the applied
 * bus/master gain slews toward its target at full traverse per 64
 * samples (1.3 ms at 48 kHz), bounding toggle transients. Meter (P-16,
 * locks at TC-2.6.3): peak-hold decaying at 20 dB/s via a per-sample
 * multiplier from ri_pow2; taps the post-master mono sum.
 * Deferred with note (NOT silent): pan (needs a stereo master; the
 * Classic path is mono end to end) and channel inserts (ride the
 * generic RIFX wrapper; land with the GUI task).
 *
 * Render path: no allocation, no IO, no unbounded loops; kernels ri_*
 * only. Exact C signatures below are executor-defined (the spec carries
 * interface sketches only, NOT frozen ABI).
 */
#ifndef RI_MIXER_H
#define RI_MIXER_H
#include <stdint.h>

#define RI_MIX_NBUS 4u
#define RI_MIX_RAMP_SMP 64u /* zipless slew: full 0..1 traverse, samples */
#define RI_MIX_DB_PER_SEC 20.0f /* P-16 meter decay rate */
#define RI_MIX_LOG2_10 3.3219281f /* binary log of ten (meter coef) */
#define RI_MIX_SR_DEFAULT 48000.0f

/* E0 fader law (P-17): (v/127)^2; v = 0 gives exactly 0. */
float ri_fader_gain(uint8_t v);

struct RiMeter {
    float peak; /* held peak, linear amplitude */
    float coef; /* per-sample hold multiplier for 20 dB/s */
};

struct RiMixBus {
    uint8_t fader; /* 0..127 */
    uint8_t send; /* 0..127, same square law, post-fader */
    uint8_t mute; /* 0/1 */
    uint8_t solo; /* 0/1 */
    float applied; /* zipless slew state: applied bus gain */
};

struct RiMixer {
    struct RiMixBus bus[RI_MIX_NBUS];
    uint8_t master; /* 0..127, same law */
    float master_applied; /* zipless slew state */
    struct RiMeter meter; /* taps the post-master mono sum */
    float sr;
};

/* Meter. sr <= 0 falls back to 48000 (documented, deterministic). */
void ri_meter_init(struct RiMeter *t, float sr);
void ri_meter_feed(struct RiMeter *t, const float *b, uint32_t n);
float ri_meter_peak(const struct RiMeter *t);

/* Mixer. sr <= 0 falls back to 48000. Setters return 0 ok, 2 bad arg
 * (bus >= 4). ri_mix_audible returns 1 audible, 0 gated (bad bus
 * fails closed to 0). ri_mix_render overwrites out and send_out
 * (caller need not clear); NULL bus inputs read as silence. */
void ri_mix_init(struct RiMixer *m, float sr);
int ri_mix_set_fader(struct RiMixer *m, uint32_t bus, uint8_t v);
int ri_mix_set_send(struct RiMixer *m, uint32_t bus, uint8_t v);
int ri_mix_set_mute(struct RiMixer *m, uint32_t bus, uint8_t v);
int ri_mix_set_solo(struct RiMixer *m, uint32_t bus, uint8_t v);
int ri_mix_set_master(struct RiMixer *m, uint8_t v);
int ri_mix_audible(const struct RiMixer *m, uint32_t bus);
void ri_mix_render(struct RiMixer *m, const float *bus_in[RI_MIX_NBUS],
    float *out, float *send_out, uint32_t n);
#endif
