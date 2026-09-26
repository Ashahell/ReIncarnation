/* t77_autolane — §12.9c automation lanes (spec 2026-09-26 r2).
 * Task 1 first (RED: only stubs exist).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/autolane.h"
#include "gui/ctlreg.h" /* cross-check ONLY (read-only; sibling-owned, never edited) */

#define T77_CAP 256u
static struct RIAutoEv T77_STO[T77_CAP];

static struct RIAutoClip T77_CLIP;

int main(void) {
    struct RIAutoLane lane;
    struct RIAutoPass pass;
    uint8_t outv;
    memset(&lane, 0, sizeof lane);
    lane.ev = T77_STO;
    lane.cap = T77_CAP;
    memset(&pass, 0, sizeof pass);
    /* Empty lane: no value anywhere. */
    RI_ASSERT(ri_auto_value(&lane, 0u, 0x0300u, &outv) == 0, "empty value");
    RI_ASSERT(lane.n == 0u && lane.cap == T77_CAP, "empty shape");
    /* Layer guard: autolane.h sees transport.h only. */
    {
        FILE *fh = fopen("engine/seq/autolane.h", "r");
        char line[256];
        int bad = 0, has_transport = 0;
        RI_ASSERT(fh != 0, "open header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "transport.h"))
                    has_transport = 1;
                /* BANNED set, permanent: project/engine-voice/scheduler
                 * types live downstream. Never delete a name here to
                 * make a build pass. */
                if (strstr(line, "RISeq") || strstr(line, "RITransport") ||
                    strstr(line, "RILoop") || strstr(line, "sched.h") ||
                    strstr(line, "clock.h") || strstr(line, "pattern.h") ||
                    strstr(line, "rbng.h") || strstr(line, "engine/dsp/") ||
                    strstr(line, "engine/fx/") || strstr(line, "mixer"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_transport, "no transport include");
        RI_ASSERT(!bad, "layer leak");
    }
    /* Layer guard 2: the emitter header adds sched/clock, nothing else. */
    {
        FILE *fh = fopen("engine/seq/autolane_emit.h", "r");
        char line[256];
        int bad = 0, has_model = 0;
        RI_ASSERT(fh != 0, "open emit header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "autolane.h\""))
                    has_model = 1;
                if (strstr(line, "RISeq") || strstr(line, "RITransport") ||
                    strstr(line, "pattern.h") || strstr(line, "rbng.h"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_model, "emit header must build on the model");
        RI_ASSERT(!bad, "emit layer leak");
    }
    /* ---- Task 1: record + punch + allow-list + value ---- */
    {
        /* Off-record refused at any cursor, lane untouched. */
        RI_ASSERT(ri_auto_touch(&lane, &pass, 0u, 0u, 96u, 0x0300u, 64u) == 2,
            "stopped refuses");
        RI_ASSERT(ri_auto_touch(&lane, &pass, 1u, 5000u, 96u, 0x0300u, 64u) == 2,
            "playing refuses");
        RI_ASSERT(lane.n == 0u && pass.npunched == 0u && pass.ntouched == 0u,
            "off-record untouched");
        /* RECORD at a grid tick writes + punches (touch set). */
        RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 48u, 96u, 0x0300u, 64u) == 0,
            "rec grid rc");
        RI_ASSERT(lane.n == 1u, "rec grid stored");
        RI_ASSERT(pass.npunched == 1u && pass.ntouched == 1u, "touch punched");
        RI_ASSERT(ri_auto_value(&lane, 48u, 0x0300u, &outv) == 1 && outv == 64u,
            "value at touch");
        /* Mid-32nd quantizes forward (12-tick grid at ppq 96). */
        RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 50u, 96u, 0x0301u, 32u) == 0,
            "rec mid rc");
        RI_ASSERT(ri_auto_value(&lane, 60u, 0x0301u, &outv) == 1 && outv == 32u,
            "mid stored next");
        RI_ASSERT(ri_auto_value(&lane, 59u, 0x0301u, &outv) == 0,
            "nothing before the line");
        /* Grid tick keeps itself. */
        RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 60u, 96u, 0x0301u, 33u) == 0,
            "rec line rc");
        RI_ASSERT(ri_auto_value(&lane, 60u, 0x0301u, &outv) == 1 && outv == 33u,
            "line keeps tick");
        /* ppq with no integer 32nd is refused. */
        RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 48u, 4u, 0x0300u, 64u) == 2,
            "ppq4 refused");
        /* Excluded IDs refused; an allowed neighbour stores. */
        RI_ASSERT(ri_auto_allowed(0x0300u) == 1, "cutoff allowed");
        RI_ASSERT(ri_auto_allowed(0x0312u) == 1, "303B allowed");
        RI_ASSERT(ri_auto_allowed(0x0401u) == 0, "808 refused");
        RI_ASSERT(ri_auto_allowed(0x1234u) == 0, "unknown refused");
        RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 72u, 96u, 0x0401u, 64u) == 2,
            "excluded refused");
        /* Replace at same (tick, ctl): no duplicate growth. */
        {
            uint32_t n0 = lane.n;
            RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 72u, 96u, 0x0302u, 10u) == 0,
                "first write rc");
            RI_ASSERT(lane.n == n0 + 1u, "first write grows");
            RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 71u, 96u, 0x0302u, 20u) == 0,
                "same-line rewrite rc");
            RI_ASSERT(lane.n == n0 + 1u, "rewrite holds count");
            RI_ASSERT(ri_auto_value(&lane, 84u, 0x0302u, &outv) == 1 && outv == 20u,
                "rewrite wins");
        }
        /* Full lane (small cap) refuses the overflow with prior intact. */
        {
            struct RIAutoLane sml;
            struct RIAutoEv ssto[2];
            struct RIAutoPass sp;
            memset(&sml, 0, sizeof sml);
            sml.ev = ssto;
            sml.cap = 2u;
            memset(&sp, 0, sizeof sp);
            RI_ASSERT(ri_auto_touch(&sml, &sp, 2u, 0u, 96u, 0x0300u, 1u) == 0, "fill 1");
            RI_ASSERT(ri_auto_touch(&sml, &sp, 2u, 12u, 96u, 0x0301u, 2u) == 0, "fill 2");
            RI_ASSERT(ri_auto_touch(&sml, &sp, 2u, 24u, 96u, 0x0302u, 3u) == 2, "full refuses");
            RI_ASSERT(sml.n == 2u, "full keeps prior");
            RI_ASSERT((sml.flags & RI_AUTO_FLAG_FULL) != 0u, "full flag sticky");
        }
        /* Retouch never duplicates set entries (the 64-entry bound is
         * defense-in-depth: only 16 IDs exist, so overflow is
         * unreachable through touch and needs no dedicated case). */
        {
            struct RIAutoPass rp;
            memset(&rp, 0, sizeof rp);
            RI_ASSERT(ri_auto_touch(&lane, &rp, 2u, 120u, 96u, 0x0303u, 1u) == 0, "touch 1");
            RI_ASSERT(rp.npunched == 1u && rp.ntouched == 1u, "sets hold one");
            RI_ASSERT(ri_auto_touch(&lane, &rp, 2u, 132u, 96u, 0x0303u, 2u) == 0, "retouch rc");
            RI_ASSERT(rp.npunched == 1u && rp.ntouched == 1u, "retouch no dup");
        }
        /* Stored 127 reads back as a value (no sentinel collision). */
        RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 108u, 96u, 0x0304u, 127u) == 0,
            "store 127 rc");
        RI_ASSERT(ri_auto_value(&lane, 108u, 0x0304u, &outv) == 1 && outv == 127u,
            "127 is a value");
        RI_ASSERT(ri_auto_value(&lane, 108u, 0x0305u, &outv) == 0,
            "unwritten is none");
    }
    /* Cross-check: engine allow-table vs registry automatable set
     * (spec §2.2; §5.2 list). Read-only walk over sibling gui/ctlreg.o:
     * every automatable row with a 303-block engine ID must be allowed;
     * every other automatable row must appear in the explicit
     * expected-unrecordable list below (section,index + legend — never
     * silently dropped). Any registry drift fails loudly here first. */
    {
        static const uint16_t UNREC[][2] = {
    {RI_SEC_808,0}, /* Level [808ALL RI_CTL_808_ACCENT] */
    {RI_SEC_808,1}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,2}, /* Tone [808V RI_CTL_808_TONE] */
    {RI_SEC_808,3}, /* Decay [808V RI_CTL_808_DECAY] */
    {RI_SEC_808,4}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,5}, /* Tone [808V RI_CTL_808_TUNE] */
    {RI_SEC_808,6}, /* Snappy [808V RI_CTL_808_SNAPPY] */
    {RI_SEC_808,7}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,8}, /* Tune [808V RI_CTL_808_TUNE] */
    {RI_SEC_808,9}, /* Switch [NONE 0] */
    {RI_SEC_808,10}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,11}, /* Tune [808V RI_CTL_808_TUNE] */
    {RI_SEC_808,12}, /* Switch [NONE 0] */
    {RI_SEC_808,13}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,14}, /* Tune [808V RI_CTL_808_TUNE] */
    {RI_SEC_808,15}, /* Switch [NONE 0] */
    {RI_SEC_808,16}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,17}, /* Switch [NONE 0] */
    {RI_SEC_808,18}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,19}, /* Switch [NONE 0] */
    {RI_SEC_808,20}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,21}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,22}, /* Tone [808V RI_CTL_808_TONE] */
    {RI_SEC_808,23}, /* Decay [808V RI_CTL_808_DECAY] */
    {RI_SEC_808,24}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,25}, /* Decay [808V RI_CTL_808_DECAY] */
    {RI_SEC_808,26}, /* Level [808V RI_CTL_808_LEVEL] */
    {RI_SEC_808,27}, /* Instrument Selection [NONE 0] */
    {RI_SEC_909,0}, /* Level [NONE 0] */
    {RI_SEC_909,1}, /* Level [909V RI_CTL_909_LEVEL] */
    {RI_SEC_909,2}, /* Tune [909V RI_CTL_909_TUNE] */
    {RI_SEC_909,3}, /* Attack [NONE 0] */
    {RI_SEC_909,4}, /* Decay [909V RI_CTL_909_DECAY] */
    {RI_SEC_909,5}, /* Level [909V RI_CTL_909_LEVEL] */
    {RI_SEC_909,6}, /* Tune [909V RI_CTL_909_TUNE] */
    {RI_SEC_909,7}, /* Tone [NONE 0] */
    {RI_SEC_909,8}, /* Snappy [NONE 0] */
    {RI_SEC_909,9}, /* Level [909V RI_CTL_909_LEVEL] */
    {RI_SEC_909,10}, /* Tune [909V RI_CTL_909_TUNE] */
    {RI_SEC_909,11}, /* Decay [909V RI_CTL_909_DECAY] */
    {RI_SEC_909,12}, /* Level [909V RI_CTL_909_LEVEL] */
    {RI_SEC_909,13}, /* Tune [909V RI_CTL_909_TUNE] */
    {RI_SEC_909,14}, /* Decay [909V RI_CTL_909_DECAY] */
    {RI_SEC_909,15}, /* Level [909V RI_CTL_909_LEVEL] */
    {RI_SEC_909,16}, /* Tune [909V RI_CTL_909_TUNE] */
    {RI_SEC_909,17}, /* Decay [909V RI_CTL_909_DECAY] */
    {RI_SEC_909,18}, /* Level [909HAT 0] */
    {RI_SEC_909,19}, /* Level [909V RI_CTL_909_LEVEL] */
    {RI_SEC_909,20}, /* Level [909V RI_CTL_909_LEVEL] */
    {RI_SEC_909,21}, /* Decay [909V RI_CTL_909_DECAY] */
    {RI_SEC_909,22}, /* Decay [909V RI_CTL_909_DECAY] */
    {RI_SEC_909,23}, /* Level [909V RI_CTL_909_LEVEL] */
    {RI_SEC_909,24}, /* Tune [909V RI_CTL_909_TUNE] */
    {RI_SEC_909,25}, /* Level [909V RI_CTL_909_LEVEL] */
    {RI_SEC_909,26}, /* Tune [909V RI_CTL_909_TUNE] */
    {RI_SEC_909,27}, /* Flam [NONE 0] */
    {RI_SEC_909,28}, /* Instrument Selection [NONE 0] */
    {RI_SEC_MIX_SYNTH1,2}, /* Level [NONE 0] */
    {RI_SEC_MIX_SYNTH1,3}, /* Pan [PAN 0] */
    {RI_SEC_MIX_SYNTH1,4}, /* Delay [SEND 0] */
    {RI_SEC_MIX_SYNTH1,5}, /* Dist [INSERT RI_ROUTE_DIST] */
    {RI_SEC_MIX_SYNTH1,6}, /* PCF [INSERT RI_ROUTE_PCF] */
    {RI_SEC_MIX_SYNTH1,7}, /* Comp [INSERT RI_ROUTE_COMP] */
    {RI_SEC_MIX_SYNTH2,2}, /* Level [NONE 0] */
    {RI_SEC_MIX_SYNTH2,3}, /* Pan [PAN 0] */
    {RI_SEC_MIX_SYNTH2,4}, /* Delay [SEND 0] */
    {RI_SEC_MIX_SYNTH2,5}, /* Dist [INSERT RI_ROUTE_DIST] */
    {RI_SEC_MIX_SYNTH2,6}, /* PCF [INSERT RI_ROUTE_PCF] */
    {RI_SEC_MIX_SYNTH2,7}, /* Comp [INSERT RI_ROUTE_COMP] */
    {RI_SEC_MIX_808,2}, /* Level [NONE 0] */
    {RI_SEC_MIX_808,3}, /* Pan [PAN 0] */
    {RI_SEC_MIX_808,4}, /* Delay [SEND 0] */
    {RI_SEC_MIX_808,5}, /* Dist [INSERT RI_ROUTE_DIST] */
    {RI_SEC_MIX_808,6}, /* PCF [INSERT RI_ROUTE_PCF] */
    {RI_SEC_MIX_808,7}, /* Comp [INSERT RI_ROUTE_COMP] */
    {RI_SEC_MIX_909,2}, /* Level [NONE 0] */
    {RI_SEC_MIX_909,3}, /* Pan [PAN 0] */
    {RI_SEC_MIX_909,4}, /* Delay [SEND 0] */
    {RI_SEC_MIX_909,5}, /* Dist [INSERT RI_ROUTE_DIST] */
    {RI_SEC_MIX_909,6}, /* PCF [INSERT RI_ROUTE_PCF] */
    {RI_SEC_MIX_909,7}, /* Comp [INSERT RI_ROUTE_COMP] */
    {RI_SEC_MASTER,3}, /* Comp [INSERT RI_ROUTE_COMP] */
    {RI_SEC_PCF,0}, /* On/Off [NONE 0] */
    {RI_SEC_PCF,2}, /* Pattern [FX RI_FXID_PCF_PATTERN] */
    {RI_SEC_PCF,3}, /* Mode [FX RI_FXID_PCF_MODE] */
    {RI_SEC_PCF,4}, /* Freq [FX RI_FXID_PCF_BASE] */
    {RI_SEC_PCF,5}, /* Q [FX RI_FXID_PCF_Q] */
    {RI_SEC_PCF,6}, /* Amt [FX RI_FXID_PCF_AMT] */
    {RI_SEC_PCF,7}, /* Decay [FX RI_FXID_PCF_DECAY] */
    {RI_SEC_DELAY,0}, /* On/Off [NONE 0] */
    {RI_SEC_DELAY,2}, /* Steps [FX RI_FXID_DELAY_STEPS] */
    {RI_SEC_DELAY,3}, /* Triplet [FX RI_FXID_DELAY_TRIPLET] */
    {RI_SEC_DELAY,4}, /* Pan [FX RI_FXID_DELAY_RETPAN] */
    {RI_SEC_DELAY,5}, /* F.Back [FX RI_FXID_DELAY_FB] */
    {RI_SEC_DIST,0}, /* On/Off [NONE 0] */
    {RI_SEC_DIST,2}, /* Amount [FX RI_FXID_DIST_DRIVE] */
    {RI_SEC_DIST,3}, /* Shape [FX RI_FXID_DIST_SHAPE] */
    {RI_SEC_COMP,0}, /* On/Off [NONE 0] */
    {RI_SEC_COMP,2}, /* Ratio [FX RI_FXID_COMP_RATIO] */
    {RI_SEC_COMP,3}, /* Threshold [FX RI_FXID_COMP_THRESH] */
    {RI_SEC_PAT_SYNTH1,0}, /* Section Off [NONE 0] */
    {RI_SEC_PAT_SYNTH1,1}, /* Bank [NONE 0] */
    {RI_SEC_PAT_SYNTH1,2}, /* Pattern [NONE 0] */
    {RI_SEC_PAT_SYNTH2,0}, /* Section Off [NONE 0] */
    {RI_SEC_PAT_SYNTH2,1}, /* Bank [NONE 0] */
    {RI_SEC_PAT_SYNTH2,2}, /* Pattern [NONE 0] */
    {RI_SEC_PAT_808,0}, /* Section Off [NONE 0] */
    {RI_SEC_PAT_808,1}, /* Bank [NONE 0] */
    {RI_SEC_PAT_808,2}, /* Pattern [NONE 0] */
    {RI_SEC_PAT_909,0}, /* Section Off [NONE 0] */
    {RI_SEC_PAT_909,1}, /* Bank [NONE 0] */
    {RI_SEC_PAT_909,2}, /* Pattern [NONE 0] */
        };
        {
            uint32_t nc = ri_ctlreg_count();
            uint32_t nq = (uint32_t)(sizeof UNREC / sizeof UNREC[0]);
            uint32_t k, q, mapped = 0u, listed = 0u;
            RI_ASSERT(nq == 112u, "unrecordable census size %u", nq);
            for (k = 0u; k < nc; k++) {
                const struct RICtlDef *d = ri_ctlreg_at(k);
                int found;
                RI_ASSERT(d != 0, "registry null row %u", k);
                if (!d || !d->automatable)
                    continue;
                if (((uint32_t)d->engine_id & 0xFF00u) == 0x0300u) {
                    RI_ASSERT(ri_auto_allowed(d->engine_id) == 1,
                        "mapped not allowed");
                    mapped++;
                    continue;
                }
                found = 0;
                for (q = 0u; q < nq; q++)
                    if (UNREC[q][0] == d->section &&
                        UNREC[q][1] == (uint16_t)(d->reg_id & 0xFFu)) {
                        found = 1;
                        break;
                    }
                RI_ASSERT(found, "unlisted automatable");
                listed++;
            }
            RI_ASSERT(mapped == 14u, "mapped count %u", mapped);
            RI_ASSERT(listed == 112u, "listed count %u", listed);
        }
    }
    /* ---- Task 2: sweeps, steps, loops, edits, stamp, clear, copy, cut/paste ---- */
    {
        struct RIAutoLane lane;
        struct RIAutoPass pass;
        struct RIAutoEv sto[32];
        uint8_t outv, vals[4];
        memset(&lane, 0, sizeof lane);
        lane.ev = sto;
        lane.cap = 32u;
        memset(&pass, 0, sizeof pass);
        /* ---- Task 2a: sweep marker law + pass end + step/loop/seek ----
         * Caller order per update is sweep-then-touch; a touch landing
         * exactly on the sweep end meets its own re-anchor there, so the
         * anchor replaces it in place (one event, latest value, marked). */
        RI_ASSERT(ri_auto_stamp(&lane, 100u, 0x0301u, 9u) == 0, "setup stamp rc");
        RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 95u, 96u, 0x0300u, 5u) == 0,
            "sweep touch rc");
        vals[0] = 7u;
        RI_ASSERT(ri_auto_sweep(&lane, &pass, 48u, 96u, vals) == 0, "sweep rc");
        RI_ASSERT(lane.n == 2u, "touch replaced in place %u", lane.n);
        RI_ASSERT(ri_auto_value(&lane, 200u, 0x0300u, &outv) == 1 && outv == 7u,
            "anchor holds latest");
        RI_ASSERT(ri_auto_value(&lane, 200u, 0x0301u, &outv) == 1 && outv == 9u,
            "untouched keeps events");
        {
            uint32_t q2;
            int marked = 0;
            for (q2 = 0u; q2 < lane.n; q2++)
                if (lane.ev[q2].tick == 96u && lane.ev[q2].ctl == 0x0300u &&
                    (lane.ev[q2].pad & RI_AUTO_EV_PASS) != 0u)
                    marked = 1;
            RI_ASSERT(marked, "re-anchor is marked");
        }
        /* Erase runs with no new move (idempotent re-sweep). */
        RI_ASSERT(ri_auto_sweep(&lane, &pass, 48u, 96u, vals) == 0, "resweep rc");
        RI_ASSERT(lane.n == 2u, "resweep count %u", lane.n);
        /* Pass's own writes survive a later sweep (marker spares them). */
        RI_ASSERT(ri_auto_sweep(&lane, &pass, 48u, 200u, vals) == 0, "later sweep rc");
        RI_ASSERT(lane.n == 3u, "later count %u", lane.n);
        RI_ASSERT(ri_auto_value(&lane, 150u, 0x0300u, &outv) == 1 && outv == 7u,
            "touch survives later sweep");
        RI_ASSERT(ri_auto_value(&lane, 200u, 0x0300u, &outv) == 1 && outv == 7u,
            "later anchor present");
        /* from>=to is a no-op (rc 0, lane identical). */
        RI_ASSERT(ri_auto_sweep(&lane, &pass, 200u, 200u, vals) == 0, "empty sweep rc");
        RI_ASSERT(ri_auto_sweep(&lane, &pass, 300u, 200u, vals) == 0, "backward sweep rc");
        RI_ASSERT(lane.n == 3u, "noop count %u", lane.n);
        /* Record without playback: stopped + RECORD writes + punches. */
        {
            struct RIAutoLane l2;
            struct RIAutoEv s2[8];
            struct RIAutoPass p2;
            memset(&l2, 0, sizeof l2);
            l2.ev = s2;
            l2.cap = 8u;
            memset(&p2, 0, sizeof p2);
            RI_ASSERT(ri_auto_touch(&l2, &p2, 2u, 1000u, 96u, 0x0300u, 44u) == 0,
                "stopped rec rc");
            RI_ASSERT(l2.n == 1u, "stopped rec stored");
            RI_ASSERT(p2.npunched == 1u, "stopped rec punches");
        }
        /* Stop ends the pass: punch-out-all keeps touched, frees punched
         * (loop wrap and backward moves punch out the same way). */
        ri_auto_punch_out_all(&pass);
        RI_ASSERT(pass.npunched == 0u && pass.ntouched == 1u, "punch-out keeps touched");
        vals[0] = 8u;
        RI_ASSERT(ri_auto_sweep(&lane, &pass, 48u, 96u, vals) == 0, "postsweep rc");
        RI_ASSERT(ri_auto_sweep(&lane, &pass, 48u, 96u, 0) == 0, "null vals rc");
        RI_ASSERT(ri_auto_value(&lane, 200u, 0x0300u, &outv) == 1 && outv == 7u,
            "freed control stands");
        /* Retouch re-punches after Stop. */
        RI_ASSERT(ri_auto_touch(&lane, &pass, 2u, 300u, 96u, 0x0300u, 8u) == 0,
            "retouch rc");
        RI_ASSERT(pass.npunched == 1u, "retouch punches");
        /* pass_end clears markers + both sets; formerly-marked events
         * stand (nothing punched to erase them). */
        {
            uint32_t q2;
            int marked = 0;
            uint32_t n0 = lane.n;
            ri_auto_pass_end(&lane, &pass);
            RI_ASSERT(pass.npunched == 0u && pass.ntouched == 0u, "pass end clears sets");
            for (q2 = 0u; q2 < lane.n; q2++)
                if (lane.ev[q2].pad & RI_AUTO_EV_PASS)
                    marked = 1;
            RI_ASSERT(!marked, "pass end clears markers");
            RI_ASSERT(ri_auto_sweep(&lane, &pass, 48u, 200u, vals) == 0, "post-end sweep rc");
            RI_ASSERT(lane.n == n0, "post-end count %u", lane.n);
        }
        /* Step record: touch in bar, sweep the bar — old bar events go,
         * touch stands, anchor lands at the bar end with the held value. */
        {
            struct RIAutoLane l3;
            struct RIAutoEv s3[8];
            struct RIAutoPass p3;
            uint8_t v3[1];
            uint32_t q3, mid = 0u;
            memset(&l3, 0, sizeof l3);
            l3.ev = s3;
            l3.cap = 8u;
            memset(&p3, 0, sizeof p3);
            RI_ASSERT(ri_auto_stamp(&l3, 100u, 0x0300u, 60u) == 0, "step old rc");
            RI_ASSERT(ri_auto_touch(&l3, &p3, 2u, 10u, 96u, 0x0300u, 50u) == 0,
                "step touch rc");
            v3[0] = 50u;
            RI_ASSERT(ri_auto_sweep(&l3, &p3, 0u, 384u, v3) == 0, "step sweep rc");
            RI_ASSERT(l3.n == 2u, "step touch plus anchor %u", l3.n);
            RI_ASSERT(ri_auto_value(&l3, 383u, 0x0300u, &outv) == 1 && outv == 50u,
                "step holds measure");
            for (q3 = 0u; q3 < l3.n; q3++)
                if (l3.ev[q3].tick > 12u && l3.ev[q3].tick < 384u)
                    mid = 1u;
            RI_ASSERT(!mid, "step bar interior clean");
        }
        /* Punch-out before advancing: touch stands, old bar events stand
         * (nobody punched to erase them), and no anchor is added. */
        {
            struct RIAutoLane l4;
            struct RIAutoEv s4[8];
            struct RIAutoPass p4;
            uint32_t q4, at768 = 0u;
            memset(&l4, 0, sizeof l4);
            l4.ev = s4;
            l4.cap = 8u;
            memset(&p4, 0, sizeof p4);
            RI_ASSERT(ri_auto_stamp(&l4, 500u, 0x0301u, 61u) == 0, "short old rc");
            RI_ASSERT(ri_auto_touch(&l4, &p4, 2u, 390u, 96u, 0x0301u, 60u) == 0,
                "short touch rc");
            ri_auto_punch_out_all(&p4);
            {
                uint8_t v4[1] = { 60u };
                RI_ASSERT(ri_auto_sweep(&l4, &p4, 384u, 768u, v4) == 0,
                    "short sweep rc");
            }
            RI_ASSERT(l4.n == 2u, "short touch plus old %u", l4.n);
            for (q4 = 0u; q4 < l4.n; q4++)
                if (l4.ev[q4].tick == 768u)
                    at768 = 1u;
            RI_ASSERT(!at768, "no anchor without punch");
        }
        /* Touch after the wrap punches in again and erases this lap. */
        {
            struct RIAutoLane lw;
            struct RIAutoEv sw[8];
            struct RIAutoPass pw;
            uint8_t vw[1];
            uint32_t qw, midw = 0u;
            memset(&lw, 0, sizeof lw);
            lw.ev = sw;
            lw.cap = 8u;
            memset(&pw, 0, sizeof pw);
            RI_ASSERT(ri_auto_stamp(&lw, 450u, 0x0300u, 70u) == 0, "wrap old rc");
            RI_ASSERT(ri_auto_touch(&lw, &pw, 2u, 500u, 96u, 0x0300u, 71u) == 0,
                "wrap touch rc");
            vw[0] = 71u;
            RI_ASSERT(ri_auto_sweep(&lw, &pw, 400u, 600u, vw) == 0, "wrap sweep rc");
            RI_ASSERT(lw.n == 2u, "wrap touch plus anchor %u", lw.n);
            for (qw = 0u; qw < lw.n; qw++)
                if (lw.ev[qw].tick >= 400u && lw.ev[qw].tick < 600u &&
                    lw.ev[qw].tick != 504u)
                    midw = 1u;
            RI_ASSERT(!midw, "wrap span holds only the touch");
            RI_ASSERT(ri_auto_value(&lw, 700u, 0x0300u, &outv) == 1 && outv == 71u,
                "wrap anchor holds");
        }
        /* Clear loop: start-in, end-out, outside kept, empty no-op. */
        RI_ASSERT(ri_auto_stamp(&lane, 100u, 0x0302u, 11u) == 0, "clear setup rc");
        RI_ASSERT(ri_auto_stamp(&lane, 300u, 0x0301u, 9u) == 0, "clear outside rc");
        RI_ASSERT(ri_auto_clear_loop(&lane, 100u, 100u) == 0, "clear rc");
        RI_ASSERT(ri_auto_value(&lane, 500u, 0x0302u, &outv) == 0, "cleared gone");
        RI_ASSERT(ri_auto_value(&lane, 500u, 0x0301u, &outv) == 1 && outv == 9u,
            "outside kept");
        RI_ASSERT(ri_auto_clear_loop(&lane, 700u, 0u) == 0, "empty clear rc");
        /* Stamp: exact tick (no quantize), denied refused. */
        RI_ASSERT(ri_auto_stamp(&lane, 77u, 0x0303u, 21u) == 0, "stamp rc");
        RI_ASSERT(ri_auto_value(&lane, 77u, 0x0303u, &outv) == 1 && outv == 21u,
            "stamp exact");
        RI_ASSERT(ri_auto_stamp(&lane, 77u, 0x0401u, 21u) == 2, "stamp denied");
        /* Init-song composition: clear all + stamp per knob at tick 0. */
        {
            struct RIAutoLane l5;
            struct RIAutoEv s5[8];
            memset(&l5, 0, sizeof l5);
            l5.ev = s5;
            l5.cap = 8u;
            RI_ASSERT(ri_auto_stamp(&l5, 0u, 0x0300u, 61u) == 0, "song stamp A");
            RI_ASSERT(ri_auto_stamp(&l5, 0u, 0x0312u, 62u) == 0, "song stamp B");
            RI_ASSERT(l5.n == 2u, "song two events");
            RI_ASSERT(ri_auto_value(&l5, 999u * 384u, 0x0300u, &outv) == 1 && outv == 61u,
                "song stamp holds");
        }
        /* Init-loop composition: clear range + stamp at loop start. */
        {
            struct RIAutoLane l6;
            struct RIAutoEv s6[8];
            memset(&l6, 0, sizeof l6);
            l6.ev = s6;
            l6.cap = 8u;
            RI_ASSERT(ri_auto_stamp(&l6, 100u, 0x0300u, 5u) == 0, "loop pre rc");
            RI_ASSERT(ri_auto_stamp(&l6, 500u, 0x0300u, 6u) == 0, "loop in rc");
            RI_ASSERT(ri_auto_clear_loop(&l6, 384u, 384u) == 0, "loop clear rc");
            RI_ASSERT(ri_auto_stamp(&l6, 384u, 0x0300u, 7u) == 0, "loop stamp rc");
            RI_ASSERT(ri_auto_value(&l6, 1000u, 0x0300u, &outv) == 1 && outv == 7u,
                "loop stamp wins");
            RI_ASSERT(ri_auto_value(&l6, 100u, 0x0300u, &outv) == 1 && outv == 5u,
                "pre-loop kept");
        }
        /* Copy touched: only touched controls, range clear + start event. */
        {
            struct RIAutoLane l7;
            struct RIAutoEv s7[16];
            struct RIAutoPass p7;
            uint8_t v7[2];
            memset(&l7, 0, sizeof l7);
            l7.ev = s7;
            l7.cap = 16u;
            memset(&p7, 0, sizeof p7);
            RI_ASSERT(ri_auto_touch(&l7, &p7, 2u, 48u, 96u, 0x0300u, 1u) == 0, "ct touch A");
            RI_ASSERT(ri_auto_touch(&l7, &p7, 2u, 60u, 96u, 0x0301u, 2u) == 0, "ct touch B");
            RI_ASSERT(ri_auto_stamp(&l7, 600u, 0x0302u, 3u) == 0, "ct other rc");
            RI_ASSERT(ri_auto_stamp(&l7, 600u, 0x0300u, 4u) == 0, "ct old A rc");
            v7[0] = 11u; v7[1] = 12u;
            RI_ASSERT(ri_auto_copy_touched(&l7, &p7, 500u, 800u, v7) == 0, "ct copy rc");
            RI_ASSERT(ri_auto_value(&l7, 799u, 0x0300u, &outv) == 1 && outv == 11u,
                "ct A at start");
            RI_ASSERT(ri_auto_value(&l7, 799u, 0x0301u, &outv) == 1 && outv == 12u,
                "ct B at start");
            RI_ASSERT(ri_auto_value(&l7, 799u, 0x0302u, &outv) == 1 && outv == 3u,
                "ct other kept");
        }
        /* Cut/copy/paste mirror bar-for-bar (ppq 96: bar is 384 ticks). */
        {
            struct RIAutoLane l8;
            struct RIAutoEv s8[16];
            memset(&l8, 0, sizeof l8);
            l8.ev = s8;
            l8.cap = 16u;
            memset(&T77_CLIP, 0, sizeof T77_CLIP);
            RI_ASSERT(ri_auto_stamp(&l8, 100u, 0x0300u, 1u) == 0, "bar pre rc");
            RI_ASSERT(ri_auto_stamp(&l8, 400u, 0x0300u, 2u) == 0, "bar in rc");
            RI_ASSERT(ri_auto_stamp(&l8, 800u, 0x0300u, 3u) == 0, "bar post rc");
            RI_ASSERT(ri_auto_copy(&l8, &T77_CLIP, 1u, 1u, 96u) == 0, "copy rc");
            RI_ASSERT(T77_CLIP.n == 1u, "clip one event");
            RI_ASSERT(T77_CLIP.base_tick == 384u, "clip base");
            RI_ASSERT(l8.n == 3u, "copy keeps lane");
            RI_ASSERT(ri_auto_cut(&l8, &T77_CLIP, 1u, 1u, 96u) == 0, "cut rc");
            RI_ASSERT(l8.n == 2u, "cut removes %u", l8.n);
            RI_ASSERT(ri_auto_value(&l8, 500u, 0x0300u, &outv) == 1 && outv == 3u,
                "cut shifts left");
            RI_ASSERT(ri_auto_paste(&l8, &T77_CLIP, 2u, 96u) == 0, "paste rc");
            RI_ASSERT(ri_auto_value(&l8, 900u, 0x0300u, &outv) == 1 && outv == 2u,
                "paste shifts right");
            RI_ASSERT(ri_auto_paste_replace(&l8, &T77_CLIP, 0u, 96u) == 0,
                "replace rc");
            RI_ASSERT(ri_auto_value(&l8, 100u, 0x0300u, &outv) == 1 && outv == 2u,
                "replace wins");
        }
        /* Paste past bar 999 drops (never wraps); NULL refuses. */
        {
            struct RIAutoLane l9;
            struct RIAutoEv s9[8];
            memset(&l9, 0, sizeof l9);
            l9.ev = s9;
            l9.cap = 8u;
            memset(&T77_CLIP, 0, sizeof T77_CLIP);
            T77_CLIP.base_tick = 0u;
            T77_CLIP.n = 1u;
            T77_CLIP.ev[0].tick = 400u;
            T77_CLIP.ev[0].ctl = 0x0300u;
            T77_CLIP.ev[0].val = 9u;
            T77_CLIP.ev[0].pad = 0u;
            RI_ASSERT(ri_auto_paste(&l9, &T77_CLIP, 998u, 96u) == 0, "past-end rc");
            RI_ASSERT(l9.n == 0u, "past-end dropped %u", l9.n);
            RI_ASSERT(ri_auto_cut(0, &T77_CLIP, 0u, 1u, 96u) == 2, "cut null lane");
            RI_ASSERT(ri_auto_copy(&l9, 0, 0u, 1u, 96u) == 2, "copy null clip");
            RI_ASSERT(ri_auto_paste(&l9, 0, 0u, 96u) == 2, "paste null clip");
            RI_ASSERT(ri_auto_paste_replace(&l9, 0, 0u, 96u) == 2, "replace null clip");
        }
        /* Paste is all-or-nothing on capacity. */
        {
            struct RIAutoLane l10;
            struct RIAutoEv s10[2];
            memset(&l10, 0, sizeof l10);
            l10.ev = s10;
            l10.cap = 2u;
            memset(&T77_CLIP, 0, sizeof T77_CLIP);
            RI_ASSERT(ri_auto_stamp(&l10, 0u, 0x0300u, 1u) == 0, "cap pre 1");
            RI_ASSERT(ri_auto_stamp(&l10, 384u, 0x0300u, 2u) == 0, "cap pre 2");
            T77_CLIP.base_tick = 0u;
            T77_CLIP.n = 1u;
            T77_CLIP.ev[0].tick = 0u;
            T77_CLIP.ev[0].ctl = 0x0301u;
            T77_CLIP.ev[0].val = 3u;
            T77_CLIP.ev[0].pad = 0u;
            RI_ASSERT(ri_auto_paste(&l10, &T77_CLIP, 0u, 96u) == 2, "cap paste refuses");
            RI_ASSERT(l10.n == 2u, "cap paste untouched %u", l10.n);
            RI_ASSERT(ri_auto_value(&l10, 384u, 0x0300u, &outv) == 1 && outv == 2u,
                "cap content kept");
        }
    }
    RI_RESULT("autolane");
}
