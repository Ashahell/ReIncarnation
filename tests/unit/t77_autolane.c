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
    RI_RESULT("autolane");
}
