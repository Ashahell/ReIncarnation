/* t1_mixer — Task 11 (gate G11): mixer + RIDevice static registry.
 * Gates (brief order): E0 fader law gain=(v/127)^2 at 9 anchors ±0.5 dB;
 * full 16-combo solo/mute truth table (4 buses x normal/mute/solo/
 * solo+mute) on the predicate AND rendered; toggle zipless (transient
 * bound: single-bus step <= 0.02, settled tails exact); meter peak-hold
 * decay 20 dB/s ±10%; dummy-device register/render/unregister with zero
 * framework edits (TC-2.1.5: only the 3 registry calls + struct store);
 * sends post-fader; master + determinism (D1) + fail-closed bad args.
 * Analysis may use libm (tests/ only; engine/ stays kernels-only).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/mixer/mixer.h"
#include "engine/framework/ridevice.h"

#define SR 48000.0f

static float DC[512];
static float B0[512];
static float OUT[512], SND[512];
static float LONG[48000];
static float SIL[2048];

static void fill(float *b, uint32_t n, float v) {
    uint32_t i;
    for (i = 0; i < n; i++)
        b[i] = v;
}

/* Dummy device for the TC-2.1.5 generality proof (test-local type: the
 * framework never learns about it — no framework edits). */
static void dummy_render(void *ctx, float *out, uint32_t n, float sr) {
    uint32_t i;
    (void)ctx;
    (void)sr;
    for (i = 0; i < n; i++)
        out[i] = 0.5f;
}

int main(void) {
    static const uint8_t ANCH[9] = { 0, 16, 32, 48, 64, 80, 96, 112, 127 };
    struct RiMixer m, m2;
    struct RiMeter t;
    const float *ins[RI_MIX_NBUS];
    uint32_t i, b, s;
    int ai;

    fill(DC, 512, 1.0f);
    fill(SIL, 2048, 0.0f);
    fill(LONG, 48000, 0.0f);
    LONG[0] = 1.0f;

    /* --- 0. registry: static table of 4, exact names --- */
    ri_devices_init();
    RI_ASSERT(ri_device_count() == 4u, "count %u want 4",
        ri_device_count());
    RI_ASSERT(ri_device_get(0) && strcmp(ri_device_get(0)->name, "303A") == 0,
        "slot0 name");
    RI_ASSERT(ri_device_get(1) && strcmp(ri_device_get(1)->name, "303B") == 0,
        "slot1 name");
    RI_ASSERT(ri_device_get(2) && strcmp(ri_device_get(2)->name, "808") == 0,
        "slot2 name");
    RI_ASSERT(ri_device_get(3) && strcmp(ri_device_get(3)->name, "909") == 0,
        "slot3 name");
    RI_ASSERT(ri_device_get(4) == 0, "slot4 must be NULL");
    ri_devices_init(); /* idempotent */
    RI_ASSERT(ri_device_count() == 4u, "re-init count");

    /* --- 1. fader law: 9 anchors ±0.5 dB --- */
    for (i = 0; i < 9; i++) {
        uint8_t v = ANCH[i];
        float g = ri_fader_gain(v);
        if (v == 0) {
            RI_ASSERT(g == 0.0f, "v=0 gain %g want exact 0", (double)g);
        } else {
            double want_db = 40.0 * log10((double)v / 127.0);
            double got_db = 20.0 * log10((double)g);
            double err = fabs(got_db - want_db);
            RI_ASSERT(err <= 0.5, "v=%u got=%.4g dB want=%.4g (err %.4g)",
                v, got_db, want_db, err);
            if (i == 1 || i == 8)
                printf("anchor v=%u: gain=%.6g (%.4g dB, want %.4g)\n",
                    v, (double)g, got_db, want_db);
        }
    }
    RI_ASSERT(ri_fader_gain(127) == 1.0f, "unity %g",
        (double)ri_fader_gain(127));

    /* --- 2. solo/mute truth table: 4 buses x 4 states --- */
    for (b = 0; b < RI_MIX_NBUS; b++) {
        for (s = 0; s < 4; s++) {
            int want, got;
            ri_mix_init(&m, SR);
            if (s == 1 || s == 3)
                ri_mix_set_mute(&m, b, 1);
            if (s == 2 || s == 3)
                ri_mix_set_solo(&m, b, 1);
            /* states 2,3 raise a solo: states 0,1 have none active */
            if (s < 2)
                want = (s == 0) ? 1 : 0;
            else
                want = (s == 2) ? 1 : 0;
            got = ri_mix_audible(&m, b);
            RI_ASSERT(got == want, "bus %u state %u audible %d want %d",
                b, s, got, want);
        }
    }
    /* cross-bus: solo on 2 gates 0/1/3, 2 stays open */
    ri_mix_init(&m, SR);
    ri_mix_set_solo(&m, 2, 1);
    RI_ASSERT(ri_mix_audible(&m, 0) == 0, "solo gates bus0");
    RI_ASSERT(ri_mix_audible(&m, 1) == 0, "solo gates bus1");
    RI_ASSERT(ri_mix_audible(&m, 2) == 1, "solo keeps bus2");
    RI_ASSERT(ri_mix_audible(&m, 3) == 0, "solo gates bus3");
    ri_mix_set_mute(&m, 2, 1);
    RI_ASSERT(ri_mix_audible(&m, 2) == 0, "mute wins over solo");
    /* rendered: all-muted DC in -> exact silence tail */
    ri_mix_init(&m, SR);
    for (b = 0; b < RI_MIX_NBUS; b++)
        ri_mix_set_mute(&m, b, 1);
    ins[0] = DC;
    ins[1] = DC;
    ins[2] = DC;
    ins[3] = DC;
    ri_mix_render(&m, ins, OUT, SND, 512);
    RI_ASSERT(OUT[511] == 0.0f, "all-mute tail %g", (double)OUT[511]);
    /* rendered: solo bus2 over DC -> tail exactly bus2 x master */
    ri_mix_init(&m, SR);
    ri_mix_set_solo(&m, 2, 1);
    ri_mix_render(&m, ins, OUT, SND, 512);
    RI_ASSERT(fabs((double)OUT[511] - 1.0) < 1e-6, "solo tail %g",
        (double)OUT[511]);

    /* --- 3. zipless toggles: transient bound + exact settle --- */
    ri_mix_init(&m, SR);
    ri_mix_set_mute(&m, 1, 1);
    ri_mix_set_mute(&m, 2, 1);
    ri_mix_set_mute(&m, 3, 1);
    ri_mix_render(&m, ins, OUT, SND, 256); /* settle bus0 at 1.0 */
    RI_ASSERT(fabs((double)OUT[255] - 1.0) < 1e-6, "settle %g",
        (double)OUT[255]);
    ri_mix_set_mute(&m, 0, 1); /* single-bus toggle off */
    ri_mix_render(&m, ins, OUT, SND, 512);
    {
        float worst = 0.0f;
        for (i = 1; i < 512; i++) {
            float d = OUT[i] - OUT[i - 1];
            if (d < 0.0f)
                d = -d;
            if (d > worst)
                worst = d;
        }
        RI_ASSERT(worst <= 0.02f, "mute transient %g > 0.02", (double)worst);
        printf("mute transient worst step: %.6g\n", (double)worst);
    }
    RI_ASSERT(OUT[511] == 0.0f, "mute tail %g", (double)OUT[511]);
    ri_mix_set_mute(&m, 0, 0); /* toggle back on */
    ri_mix_render(&m, ins, OUT, SND, 512);
    {
        float worst = 0.0f;
        for (i = 1; i < 512; i++) {
            float d = OUT[i] - OUT[i - 1];
            if (d < 0.0f)
                d = -d;
            if (d > worst)
                worst = d;
        }
        RI_ASSERT(worst <= 0.02f, "unmute transient %g > 0.02",
            (double)worst);
    }
    RI_ASSERT(fabs((double)OUT[511] - 1.0) < 1e-6, "unmute tail %g",
        (double)OUT[511]);

    /* --- 4. meter: peak-hold + 20 dB/s ±10% --- */
    ri_meter_init(&t, SR);
    ri_meter_feed(&t, LONG, 48000); /* 1.0 then 1 s of silence */
    {
        double rate = -20.0 * log10((double)ri_meter_peak(&t));
        RI_ASSERT(fabs(rate - 20.0) <= 2.0, "decay %.4g dB/s want 20±2",
            rate);
        printf("meter: 1 s decay to %.6g (rate %.4g dB/s)\n",
            (double)ri_meter_peak(&t), rate);
    }
    ri_meter_init(&t, SR);
    fill(B0, 512, 0.9f);
    ri_meter_feed(&t, B0, 64); /* hot peak holds... */
    RI_ASSERT(fabs((double)ri_meter_peak(&t) - 0.9) < 1e-6, "hold %g",
        (double)ri_meter_peak(&t));
    fill(B0, 512, 0.3f);
    ri_meter_feed(&t, B0, 64); /* ...cold feed does not pull it down */
    RI_ASSERT(ri_meter_peak(&t) > 0.85f, "cold pull %g",
        (double)ri_meter_peak(&t));
    ri_meter_feed(&t, SIL, 2048); /* long silence still finite */
    RI_ASSERT(ri_meter_peak(&t) == ri_meter_peak(&t) &&
        ri_meter_peak(&t) < 1.0f, "meter finite %g",
        (double)ri_meter_peak(&t));

    /* --- 5. dummy device: register/render/unregister, no edits --- */
    {
        struct RIDevice *slot = ri_device_get(3);
        struct RIDevice saved = *slot;
        float buf[256];
        slot->render = dummy_render;
        slot->ctx = 0;
        slot->render(slot->ctx, buf, 256, SR);
        ai = 1;
        for (i = 0; i < 256; i++) {
            if (buf[i] != 0.5f) {
                ai = 0;
                break;
            }
        }
        RI_ASSERT(ai, "dummy render pattern");
        *slot = saved; /* unregister: restore */
        RI_ASSERT(strcmp(slot->name, "909") == 0, "slot3 name restored");
        RI_ASSERT(slot->render == 0, "slot3 render restored");
    }

    /* --- 6. sends: silent at 0, post-fader at 127 --- */
    ri_mix_init(&m, SR);
    ri_mix_set_fader(&m, 0, 127);
    ri_mix_set_fader(&m, 1, 0);
    ri_mix_set_fader(&m, 2, 0);
    ri_mix_set_fader(&m, 3, 0);
    ins[0] = DC;
    ins[1] = 0;
    ins[2] = 0;
    ins[3] = 0;
    ri_mix_set_send(&m, 0, 127);
    ri_mix_render(&m, ins, OUT, SND, 256);
    RI_ASSERT(fabs((double)SND[255] - 1.0) < 1e-6, "send tail %g",
        (double)SND[255]);
    ri_mix_set_send(&m, 0, 0);
    ri_mix_render(&m, ins, OUT, SND, 256);
    RI_ASSERT(SND[255] == 0.0f, "send0 tail %g", (double)SND[255]);
    ri_mix_set_send(&m, 0, 127);
    ri_mix_set_mute(&m, 0, 1); /* mute gates the send: post-fader */
    ri_mix_render(&m, ins, OUT, SND, 256);
    RI_ASSERT(SND[255] == 0.0f, "muted send %g", (double)SND[255]);

    /* --- 7. master, determinism, fail-closed args --- */
    {
        static float O2[512], S2[512];
        const float *one[RI_MIX_NBUS] = { DC, 0, 0, 0 };
        const float *all[RI_MIX_NBUS] = { DC, DC, DC, DC };
        ri_mix_init(&m, SR);
        ri_mix_set_send(&m, 0, 127);
        ri_mix_render(&m, one, OUT, SND, 512);
        RI_ASSERT(fabs((double)OUT[511] - 1.0) < 1e-6, "one-bus %g",
            (double)OUT[511]);
        ri_mix_init(&m2, SR);
        ri_mix_set_send(&m2, 0, 127);
        ri_mix_render(&m2, all, O2, S2, 512);
        RI_ASSERT(fabs((double)O2[511] - 4.0) < 1e-5, "four-bus %g",
            (double)O2[511]);
        RI_ASSERT(memcmp(OUT, O2, sizeof OUT) != 0,
            "bus count must be audible");
        ri_mix_init(&m, SR);
        ri_mix_set_send(&m, 0, 127);
        ri_mix_render(&m, all, OUT, SND, 512);
        RI_ASSERT(memcmp(OUT, O2, sizeof OUT) == 0, "D1 identical");
        RI_ASSERT(memcmp(SND, S2, sizeof SND) == 0, "D1 send identical");
    }
    ri_mix_set_master(&m, 0);
    ri_mix_render(&m, ins, OUT, SND, 512);
    RI_ASSERT(OUT[511] == 0.0f, "master0 tail %g", (double)OUT[511]);
    RI_ASSERT(ri_mix_set_fader(&m, 4, 100) == 2, "bad bus fader");
    RI_ASSERT(ri_mix_set_send(&m, 4, 100) == 2, "bad bus send");
    RI_ASSERT(ri_mix_set_mute(&m, 4, 1) == 2, "bad bus mute");
    RI_ASSERT(ri_mix_set_solo(&m, 4, 1) == 2, "bad bus solo");
    RI_ASSERT(ri_mix_set_master(0, 100) == 2, "null master");
    RI_ASSERT(ri_mix_audible(&m, 4) == 0, "bad bus fails closed");
    RI_ASSERT(ri_mix_audible(0, 0) == 0, "null mixer fails closed");
    ri_mix_render(0, ins, OUT, SND, 8); /* null mixer: no crash */
    ri_meter_feed(0, DC, 8);
    RI_ASSERT(ri_meter_peak(0) == 0.0f, "null meter peak");

    RI_RESULT("mixer");
}
