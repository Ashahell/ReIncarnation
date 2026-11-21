/* t216_stem_fx — R8f: the FX buses, and the aliasing that hid them.
 *
 * R8e delivered one WAV per mixer strip. The spec also asked for "plus FX
 * returns and the master". The master was already the mix; the returns were
 * not, and the reason is worth stating plainly:
 *
 * **THE DELAY IS THE ONLY SEND/RETURN FX IN THIS ENGINE.** DIST, PCF and
 * COMP are in-place inserts -- `ri_fxdist_render(&e->dist, e->scratch,
 * e->scratch, cc)`, input and output the same buffer -- so the strip output
 * already IS post-FX and there is no return to tap. The shared delay is
 * different: its send is summed post-insert across the strips, rendered into
 * `retbus`, stereo-panned and folded into the master.
 *
 * **SO THE RETURN WAS IN THE MIX AND IN NO STEM.** A DAW user re-processing
 * the strips lost the delay with no way to put it back. That is the hole
 * this closes, and it is the whole of "FX returns".
 *
 * **AND THE BUG THAT HID IT WAS ALIASING, TWICE.**
 *
 *  1. **The FX STEMS REUSED A SECTION'S INTERLEAVE BUFFER.** The set stores
 *     POINTERS and copies nothing until the write, so writing `fx-return`
 *     through the 303A's buffer overwrote the 303A's audio first. The
 *     symptom was the 303A stem going **silent** the moment `--fx` was
 *     added -- a section that had been rendering at peak 20024 for three
 *     commits.
 *  2. **THE POINTERS NEVER REACHED THE ENGINE.** `r->fx[]` and
 *     `r->tap.fx[]` are two different arrays: the caller fills the first
 *     and the engine reads the second. The section pointers never had this
 *     problem because `ri_stemr_begin` fills `r->tap.l[]` directly, so the
 *     FX path quietly acquired a second way of holding a pointer and only
 *     one of them was wired. The result was files that were correctly
 *     named, correctly sized, and **empty** -- the worst of the three.
 *
 * Neither was visible without rendering, and the first one's symptom pointed
 * at the 303A rather than at the FX code.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "project/stem_render.h"

/* 256 is too short for the RETURN, and that is the FX's own arithmetic
 * rather than a tap problem: `ri_engine_set_delay` forces `mix = 1.0` (send
 * topology), so the return is the DELAYED signal alone, and the default 1/8
 * note at 120 BPM is 6000 samples. The line has to fill before the return has
 * anything in it. The window is comfortably past that so the test measures
 * THE TAP, not the delay time. */
#define FXF 8192u
/* The FX case needs a window longer than the delay; see §7b. */
#define FXW 32768u

static struct RIEngine E;
static struct RIStemr R;
static struct RIStemrSlot S[RI_ROUTE_NSECTIONS];
/* SIZED AT FXW, NOT FXF. §7b begins its bridge with frames = FXW (the delay
 * is longer than FXF), and  writes  samples into
 * every slot buffer -- so arrays sized FXF were overrun by 24576 floats,
 * straight over the statics after them. The symptom was §7b seeing zero FX
 * while a standalone program running the SAME sequence saw 32768: the code
 * was right and the FIXTURE was undersized. */
static float L[RI_ROUTE_NSECTIONS][FXW], Rr[RI_ROUTE_NSECTIONS][FXW];
static float O[RI_ROUTE_NSECTIONS][FXW * 2u];
static float FXBUF[RI_STEM_FX_N][FXW];
static float FXOUT[FXW * 2u];
static struct RIEvent EV[4];

/* The bridge under test is a PARAMETER, not the file-scope one. My first
 * version began `R` only, so every case using a local `R2`/`R3`/`R4` was
 * testing an UNINITIALISED struct -- and `ri_stemr_finish` refused it
 * correctly, which read as "the fix does not work". */
static struct RIStemr *g_r;

static void setup(uint32_t sections) {
    uint32_t i;
    for (i = 0u; i < RI_ROUTE_NSECTIONS; i++) {
        S[i].l = L[i];
        S[i].r = Rr[i];
        S[i].out = O[i];
    }
    memset(EV, 0, sizeof EV);
    EV[0].type = RI_EV_NOTE_ON; EV[0].device = 0u; EV[0].value = 36u;
    EV[0].sample = 0u;
    EV[1].type = RI_EV_NOTE_ON; EV[1].device = 2u; EV[1].value = 36u;
    EV[1].sample = 8u;
    EV[2].type = RI_EV_NOTE_ON; EV[2].device = 4u; EV[2].value = 60u;
    EV[2].sample = 16u;
    ri_engine_init(&E);
    ri_engine_defaults(&E);
    ri_engine_load(&E, EV, 3u, FXF, sections);
    ri_stemr_begin(g_r, RI_ROUTE_NSECTIONS, FXF, 48000u, STEM_WAV_PCM16,
        120000u, S);
}

static void bind_fx(void) {
    ri_stemr_fx_bind(&R, RI_STEM_FX_SEND, FXBUF[0]);
    ri_stemr_fx_bind(&R, RI_STEM_FX_RETURN, FXBUF[1]);
    ri_stemr_fx_out(&R, FXOUT);
    ri_stemr_bind(&R, &E);
}

int main(void) {
    uint32_t si, i;
    g_r = &R;

    /* --- 1. THE NAMES CONTINUE THE SECTION NUMBERING ---------------- */
    {
        char nm[32];
        RI_ASSERT(ri_stemr_fx_name(nm, sizeof nm, 6u, RI_STEM_FX_SEND) == 14u,
            "06-fx-send.wav is 14 chars (%u)",
            (unsigned)ri_stemr_fx_name(nm, sizeof nm, 6u, RI_STEM_FX_SEND));
        RI_ASSERT(strcmp(nm, "06-fx-send.wav") == 0, "it is 06-fx-send.wav (%s)",
            nm);
        RI_ASSERT(ri_stemr_fx_name(nm, sizeof nm, 7u, RI_STEM_FX_RETURN) != 0,
            "a return name");
        RI_ASSERT(strcmp(nm, "07-fx-return.wav") == 0, "07-fx-return.wav (%s)",
            nm);
        /* THE INDEX IS THE CALLER'S, not a function of the bus, so adding an
         * FX bus later does not renumber the ones before it. */
        RI_ASSERT(ri_stemr_fx_name(nm, sizeof nm, 9u, RI_STEM_FX_SEND) != 0 &&
            strncmp(nm, "09-", 3) == 0, "a caller-chosen index (%s)", nm);
        RI_ASSERT(ri_stemr_fx_name(nm, sizeof nm, 6u, 9u) == 0u,
            "an fx past the last is refused");
        RI_ASSERT(ri_stemr_fx_name(nm, 4u, 6u, RI_STEM_FX_SEND) == 0u,
            "a too-small buffer is refused");
        /* AND THEY SORT AFTER THE SECTIONS: 06 and 07 follow 05. */
        {
            char sec[32];
            ri_stemr_name(sec, sizeof sec, 5u, 4u);
            RI_ASSERT(strcmp(sec, "05-levi.wav") < strcmp(nm, "07-fx-return.wav"),
                "the last section sorts before the first FX bus");
        }
    }

    /* --- 2. NO FX ASKED FOR MEANS NO FX FILES ------------------------ */
    setup(RI_ENGINE_S303A);
    ri_stemr_bind(&R, &E);
    RI_ASSERT(ri_stemr_finish(&R, FXF) == 0u, "finish");
    RI_ASSERT(stem_set_count(&R.set) == 1u, "one stem, not three (%u)",
        (unsigned)stem_set_count(&R.set));
    RI_ASSERT(ri_stemr_fx_set_index(&R, RI_STEM_FX_SEND) == RI_STEMR_NO_STEM,
        "and no FX stem is reported");
    RI_ASSERT(ri_stemr_fx_set_index(&R, RI_STEM_FX_RETURN) == RI_STEMR_NO_STEM,
        "for either bus");

    /* --- 3. BOUND FX BUSES BECOME STEMS, AFTER THE SECTIONS ---------- */
    setup(RI_ENGINE_S303A);
    bind_fx();
    /* Something in both buses, so "is this the data or a stale buffer?" has
     * an answer that is not zero. */
    for (i = 0u; i < FXF; i++) {
        FXBUF[RI_STEM_FX_SEND][i] = 0.5f;
        FXBUF[RI_STEM_FX_RETURN][i] = -0.25f;
    }
    RI_ASSERT(ri_stemr_finish(&R, FXF) == 0u, "finish");
    RI_ASSERT(stem_set_count(&R.set) == 3u,
        "one section plus two FX (%u)", (unsigned)stem_set_count(&R.set));
    si = ri_stemr_fx_set_index(&R, RI_STEM_FX_SEND);
    RI_ASSERT(si != RI_STEMR_NO_STEM && si == 1u,
        "the send is set entry 1, AFTER the section (%u)", (unsigned)si);
    si = ri_stemr_fx_set_index(&R, RI_STEM_FX_RETURN);
    RI_ASSERT(si == 2u, "and the return is entry 2 (%u)", (unsigned)si);
    /* THE SECTION'S OWN INDEX IS UNCHANGED: a sparse mask must not shift the
     * sections when FX are added, or a caller who recorded them yesterday
     * gets different files today. */
    RI_ASSERT(ri_stemr_set_index(&R, 0u) == 0u,
        "section 0 is still entry 0 (%u)", (unsigned)ri_stemr_set_index(&R, 0u));

    /* --- 3b. AND THE SECTION'S OWN BUFFER IS UNTOUCHED ---------------- */
    /* THE ALIASING CHECK LIVES HERE, NOT IN §5. §5 tests the path where
     * `fx_out` is NULL, and the whole FX block is guarded by `if (r->fx_out)`
     * -- so a mutation that swaps `r->fx_out` for a section slot is INSIDE a
     * guard §5 never enters, and §5 cannot kill it no matter what it
     * asserts. Four attempts to kill it from §5 produced four survivors, all
     * with a different object hash: the mutation was always real and always
     * unreachable from that case.
     *
     * Here `fx_out` IS set, section 0's interleave buffer holds the section's
     * own zeros, and the FX buses hold 0.5 and -0.25. Aliasing would put
     * 0.5 into the 303A's buffer. */
    RI_ASSERT(O[0][0] == 0.0f,
        "the 303A's interleave buffer still holds ITS OWN sample (%g, not an"
        " FX value)", (double)O[0][0]);
    RI_ASSERT(FXOUT[0] != 0.0f,
        "and the FX stems went into the CALLER'S fx_out instead (%g)",
        (double)FXOUT[0]);

    /* --- 4. THE DATA IS THE DATA, AND MONO-INTO-STEREO -------------- */
    {
        static uint8_t wav[65536];
        uint32_t w = stem_set_write(&R.set, 2u, wav, sizeof wav);
        RI_ASSERT(w != 0u, "the return stem writes (%u)", (unsigned)w);
        /* 0xFF = -1 in 16-bit; -0.25 is 0xE000. BOTH channels, because a
         * mono entry among stereo ones imports with a channel count nobody
         * expected. */
        /* LITTLE-ENDIAN 16-BIT, and the quantiser rounds -- so the assertion
         * is on the LAW (both channels carry the same value, near -0.25)
         * rather than on an exact -8192, which a rounding path will not
         * produce and which would make this test about the quantiser. */
        {
            int16_t l = (int16_t)((uint16_t)wav[44] | ((uint16_t)wav[45] << 8));
            int16_t rr = (int16_t)((uint16_t)wav[46] | ((uint16_t)wav[47] << 8));
            RI_ASSERT(l == rr, "BOTH channels carry the same sample (%d, %d)",
                (int)l, (int)rr);
            RI_ASSERT(l < -8100 && l > -8250,
                "and it is about -0.25 in 16-bit (%d)", (int)l);
        }
    }

    /* --- 5. THE ALIASING BUG: A SECTION SLOT'S BUFFER IS NOT OURS ----- */
    /* The 303A stem was silent because the FX stems interleave through the
     * caller's slot buffer. `fx_out` is the ONLY buffer the FX loop writes,
     * and a NULL there means "no FX stems" rather than "use a slot". */
    {
        static struct RIStemr R2;
        static float out2[FXF * 2u];
        static float fx2[FXF];
        uint32_t before, i;
        g_r = &R2;
        setup(RI_ENGINE_S303A);
        ri_stemr_fx_bind(&R2, RI_STEM_FX_SEND, fx2);
        /* DELIBERATELY NOT CALLING ri_stemr_fx_out. */
        ri_stemr_bind(&R2, &E);
        /* THE DATA IS WRITTEN AFTER BIND, and that is the fix for a mutant
         * that survived this case. `ri_stemr_bind` ZEROES the FX buffers --
         * which is correct, and which silently deleted the very value this
         * test was watching for. So the aliasing mutation wrote zeros over the
         * 303A's buffer and left it looking untouched, and the case passed
         * for the wrong reason. A test that fills its input before the call
         * under test has not proved the call does not destroy it. */
        for (i = 0u; i < FXF; i++)
            fx2[i] = 7.0f;
        before = O[0][0];                       /* the 303A's interleave slot */
        RI_ASSERT(ri_stemr_finish(&R2, FXF) == 0u, "finish without fx_out");
        RI_ASSERT(O[0][0] == before,
            "the 303A's interleave buffer is UNTOUCHED (%g vs %g)",
            (double)O[0][0], (double)before);
        RI_ASSERT(ri_stemr_fx_set_index(&R2, RI_STEM_FX_SEND) == RI_STEMR_NO_STEM,
            "and no FX stem is reported when no buffer was given");
        (void)out2;
    }

    /* --- 6. THE POINTERS REACH THE ENGINE ---------------------------- */
    /* `r->fx[]` and `r->tap.fx[]` are DIFFERENT ARRAYS. If bind does not
     * copy, the engine sees NULL and every FX file is empty -- which is what
     * happened, and it looks like correct output. */
    {
        static struct RIStemr R3;
        static float fx3[RI_STEM_FX_N][FXF];
        g_r = &R3;
        setup(RI_ENGINE_S303A);
        ri_stemr_fx_bind(&R3, RI_STEM_FX_SEND, fx3[0]);
        ri_stemr_fx_bind(&R3, RI_STEM_FX_RETURN, fx3[1]);
        ri_stemr_fx_out(&R3, FXOUT);
        ri_stemr_bind(&R3, &E);
        RI_ASSERT(R3.tap.fx[RI_STEM_FX_SEND] == fx3[0],
            "the SEND pointer reached the tap (%p vs %p)",
            (void *)R3.tap.fx[RI_STEM_FX_SEND], (void *)fx3[0]);
        RI_ASSERT(R3.tap.fx[RI_STEM_FX_RETURN] == fx3[1],
            "and the RETURN pointer");
        RI_ASSERT(E.stems != 0, "the engine is attached");
        RI_ASSERT(E.stems && E.stems->fx[RI_STEM_FX_SEND] == fx3[0],
            "and what the ENGINE reads is the same pointer");
    }

    /* --- 7. BIND ZEROES THE FX BUFFERS -------------------------------- */
    /* The engine writes only what it renders, so without this a short render
     * leaves the previous song's delay tail at the end of a file. */
    {
        static struct RIStemr R4;
        static float fx4[FXF];
        g_r = &R4;
        setup(RI_ENGINE_S303A);
        for (i = 0u; i < FXF; i++)
            fx4[i] = 9.0f;
        ri_stemr_fx_bind(&R4, RI_STEM_FX_SEND, fx4);
        ri_stemr_fx_out(&R4, FXOUT);
        ri_stemr_bind(&R4, &E);
        RI_ASSERT(fx4[0] == 0.0f, "the send buffer is zeroed (%g)",
            (double)fx4[0]);
        RI_ASSERT(fx4[FXF - 1u] == 0.0f, "including the tail");
    }

    /* --- 7b. THE ENGINE ACTUALLY WRITES THEM ------------------------- */
    /* YA, YB and YC ALL SURVIVED, and all three are in the ENGINE's tap.
     * Every case above sets `FXBUF` by hand and calls `finish`, so the
     * engine's write was never executed -- the test proved the BRIDGE and
     * nothing about the tap that feeds it. A test that fills in the data it
     * is checking is a test that cannot see the data not arriving.
     *
     * So: a real delay line, real sends, and a real render in BLOCKS --
     * blocks because that is what YC needs, since a one-call render has
     * `done == cursor` and cannot tell them apart. */
    {
        /* THE WINDOW MUST OUTLAST THE DELAY, and that is the FX's own
         * arithmetic rather than a tap property: `ri_engine_set_delay`
         * forces `mix = 1.0` (send topology), so the return is the DELAYED
         * signal alone, and `ri_fxdelay_resync` re-derives the time from the
         * tempo every buffer -- 1/8 note at 120 BPM is 15000 samples. My
         * first window was 8192, which is shorter than the delay, and it read
         * as "the return is never tapped".
         *
         * `d->fb` defaults to 0, so the line holds the input with no
         * recirculation: the return is the input 15000 samples ago. That is
         * enough to prove the tap and short enough to stay deterministic. */
        static struct RIStemr R5;
        static float dline[65536];
        static float ml[64], mr[64];
        uint64_t off;
        uint32_t got, nz_send = 0u, nz_ret = 0u, tail = 0u;
        g_r = &R5;
        setup(RI_ENGINE_S303A);
        /* THE ENGINE'S OWN LENGTH IS THE WINDOW TOO. `ri_engine_load` sets
         * `total`, and the render stops there, so overriding only the
         * bridge's `frames` left the engine finishing at FXF -- which reads
         * as a render that stopped early rather than as two lengths that
         * have to agree. */
        /* LENGTH FIRST, THEN DELAY, SENDS, AND BIND -- in that order, each
         * for a reason. `ri_engine_load` resets the cursor and detaches the
         * tap, so it must precede `ri_stemr_bind`; `ri_engine_set_delay`
         * snapshots and restores the knob fields across its own init, so it
         * must follow the load or the load's defaults win. */
        ri_engine_load(&E, EV, 3u, FXW, RI_ENGINE_S303A);
        R5.frames = FXW;
        ri_stemr_fx_bind(&R5, RI_STEM_FX_SEND, FXBUF[RI_STEM_FX_SEND]);
        ri_stemr_fx_bind(&R5, RI_STEM_FX_RETURN, FXBUF[RI_STEM_FX_RETURN]);
        ri_stemr_fx_out(&R5, FXOUT);
        ri_engine_set_delay(&E, dline, (uint32_t)(sizeof dline / sizeof dline[0]));
        /* THE SHORTEST DELAY, because `ri_fxdelay_resync` re-derives the
         * time from `steps` and the tempo EVERY BUFFER. The default is an
         * eighth note, which at 120 BPM is 15000 samples -- inside this
         * window, but only just, and it left the return's last third empty.
         * One step is ~375 samples, so the line fills early and the whole
         * window carries a return. The point is the TAP, not the delay. */
        E.delay.steps = 1u;
        for (i = 0u; i < RI_ROUTE_NSECTIONS; i++)
            ri_engine_set_send(&E, (uint8_t)i, 100u);
        ri_stemr_bind(&R5, &E);
        /* THE RENDER IS IN BLOCKS, and the tap is checked at an ABSOLUTE
         * position past the first block -- the same law R8e exists for. */
        RI_ASSERT(E.dline != 0, "THE DELAY LINE IS ATTACHED at render");
        RI_ASSERT(E.send[0] != 0u, "and a section is SENDING (%u)", (unsigned)E.send[0]);
        for (off = 0u; off < FXW; off += 64u) {
            got = ri_engine_render(&E, ml, mr, 64u, 48000.0f);
            RI_ASSERT(got == 64u, "block at %llu rendered", (unsigned long long)off);
        }
        RI_ASSERT(E.stems != 0, "the engine is attached");
        RI_ASSERT(E.stems && E.stems->fx[RI_STEM_FX_SEND] == FXBUF[RI_STEM_FX_SEND],
            "and the engine sees the SEND buffer (%p vs %p)",
            (void *)(E.stems ? E.stems->fx[RI_STEM_FX_SEND] : 0),
            (void *)FXBUF[RI_STEM_FX_SEND]);
        for (i = 0u; i < FXW; i++) {
            if (FXBUF[RI_STEM_FX_SEND][i] != 0.0f) nz_send++;
            if (FXBUF[RI_STEM_FX_RETURN][i] != 0.0f) nz_ret++;
        }
        RI_ASSERT(nz_send > 0u,
            "the ENGINE WROTE the send bus (%u nonzero of %u)", (unsigned)nz_send,
            (unsigned)FXW);
        RI_ASSERT(nz_ret > 0u,
            "and the return bus (%u nonzero of %u)", (unsigned)nz_ret,
            (unsigned)FXW);
        /* AND THE WRITE IS PAST THE FIRST BLOCK, which is what YC breaks.
         * Checked on the SEND, which is non-zero from the first sample --
         * the return's leading silence is the delay time and saying
         * otherwise would be asserting a property of the FX, not the tap. */
        for (i = FXW / 2u; i < FXW; i++)
            if (FXBUF[RI_STEM_FX_SEND][i] != 0.0f) tail++;
        RI_ASSERT(tail > 0u,
            "the send was written in the SECOND HALF (%u)", (unsigned)tail);
        /* And the finish adds both, from data the engine produced. */
        RI_ASSERT(ri_stemr_finish(&R5, FXW) == 0u, "finish");
        RI_ASSERT(ri_stemr_fx_set_index(&R5, RI_STEM_FX_SEND) != RI_STEMR_NO_STEM,
            "and both buses are stems");
    }

    /* --- 8. NULLs ----------------------------------------------------- */
    ri_stemr_fx_bind(0, 0u, FXBUF[0]);        /* must not crash */
    ri_stemr_fx_bind(&R, 9u, FXBUF[0]);       /* must not crash */
    ri_stemr_fx_out(0, FXOUT);                 /* must not crash */
    RI_ASSERT(ri_stemr_fx_set_index(0, 0u) == RI_STEMR_NO_STEM, "NULL");
    RI_ASSERT(ri_stemr_fx_set_index(&R, 9u) == RI_STEMR_NO_STEM,
        "an fx past the last");
    ri_stemr_fx_name(0, 32u, 6u, RI_STEM_FX_SEND);

    RI_RESULT("stem-fx");
}