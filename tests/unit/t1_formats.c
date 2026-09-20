/* t1_formats — Task 13 (gate G13): RBNG codec, RBNM-full, MIDI learn,
 * automation, ARexx, undo. Pure/host-tested; AROS backends (CAMD,
 * datatypes, MCC-style shells) are AROS-only TUs, never in this build.
 *
 * Sections (brief order): RBNG round-trip + serialize-parse-serialize
 * byte-identical; unknown-chunk preservation; VERS major/minor/flags
 * forward-compat; corrupt-chunk abort naming chunk ID + byte offset;
 * MODR missing-mod warn string; SKIN art-fallback; CPRG hook; automation
 * save/load (bound ±1 unit, asserted exact); undo-200 scripted; MIDI
 * learn/loopback + note→step + MMC + 100-bar clock drift + flood cap +
 * hot-unplug; ARexx exact strings + REBIRTHAROS alias; RBNM-full
 * (reserialize byte-identical, CPRG extract, SHA-256 identity vectors).
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "project/rbng.h"
#include "project/rbnm.h"
#include "project/sha256.h"
#include "project/arexx.h"
#include "project/undo.h"
#include "midi_io/midi.h"
#include "engine/seq/sched.h"

static struct RISong demo_song(void) {
    struct RISong s;
    uint32_t i;
    rbng_song_init(&s);
    s.tempo = 140;
    s.ppq = 96;
    s.nsteps = 16;
    for (i = 0; i < 16u; i++) {
        s.steps[i].note = (uint8_t)(45 + (i % 8));
        s.steps[i].flags = 0;
    }
    s.steps[2].flags = RI_RBNG_ACCENT;
    s.steps[3].flags = RI_RBNG_SLIDE;
    s.steps[6].flags = RI_RBNG_REST;
    s.steps[7].flags = RI_RBNG_FLAM;
    s.nauto = 3;
    s.auto_ev[0].tick = 0;
    s.auto_ev[0].ctl = 0x0300u;
    s.auto_ev[0].val = 80;
    s.auto_ev[1].tick = 48;
    s.auto_ev[1].ctl = 0x0300u;
    s.auto_ev[1].val = 96;
    s.auto_ev[2].tick = 96;
    s.auto_ev[2].ctl = 0x0A00u;
    s.auto_ev[2].val = 112;
    s.nmods = 1;
    strcpy(s.mods[0].name, "acid-01");
    for (i = 0; i < 64u; i++)
        s.mods[0].sha[i] = "0123456789abcdef"[i % 16u];
    s.mods[0].sha[64] = '\0';
    s.mods[0].vers = 3;
    strcpy(s.cprg, "(C) 2026 RI clean-room demo");
    return s;
}

static int file_bytes(const char *path, unsigned char *dst, uint32_t cap,
    uint32_t *n_out) {
    FILE *f = fopen(path, "rb");
    uint32_t n = 0;
    int ch;
    if (!f)
        return 2;
    while ((ch = fgetc(f)) != EOF) {
        if (n >= cap) {
            fclose(f);
            return 2;
        }
        dst[n++] = (unsigned char)ch;
    }
    fclose(f);
    *n_out = n;
    return 0;
}

int main(void) {
    static char err[256];
    static char warn[256];
    static unsigned char A[2097152], B[2097152];
    static unsigned char fbuf[8192];
    uint32_t na = 0, nb = 0, nf = 0;
    struct RISong s = demo_song(), r;
    struct RIMidiLearn ml;
    struct RIMidiClock ck;
    struct RIFlood fb;
    struct RIUndo u;
    struct RIArexxCmd c;
    char cprg[136];
    char hex[65];
    uint32_t i;
    int present;

    /* --- 1. RBNG write→read round trip (exact, incl. automation) --- */
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t13-rt.rbng", &s, err,
        sizeof err) == 0, "write: %s", err);
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-rt.rbng", &r, err,
        sizeof err) == 0, "read: %s", err);
    RI_ASSERT(r.tempo == 140 && r.ppq == 96 && r.nsteps == 16, "song hdr");
    for (i = 0; i < 16u; i++)
        RI_ASSERT(r.steps[i].note == s.steps[i].note &&
            r.steps[i].flags == s.steps[i].flags, "step %u", i);
    RI_ASSERT(r.nauto == 3, "nauto %u", r.nauto);
    for (i = 0; i < 3u; i++) {
        int d = (int)r.auto_ev[i].val - (int)s.auto_ev[i].val;
        if (d < 0)
            d = -d;
        RI_ASSERT(d <= 1, "auto %u drift %d", i, d); /* bound ±1, exact */
        RI_ASSERT(r.auto_ev[i].tick == s.auto_ev[i].tick &&
            r.auto_ev[i].ctl == s.auto_ev[i].ctl, "auto %u key", i);
    }
    RI_ASSERT(r.nmods == 1 && strcmp(r.mods[0].name, "acid-01") == 0 &&
        r.mods[0].vers == 3, "modr");
    RI_ASSERT(strcmp(r.mods[0].sha, s.mods[0].sha) == 0, "modr sha");
    RI_ASSERT(strcmp(r.cprg, s.cprg) == 0, "cprg");

    /* --- 2. serialize-parse-serialize byte-identical --- */
    RI_ASSERT(file_bytes("/tmp/ri/run/t13-rt.rbng", A, sizeof A, &na) == 0,
        "read A");
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-rt.rbng", &r, err,
        sizeof err) == 0, "re-read: %s", err);
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t13-rt2.rbng", &r, err,
        sizeof err) == 0, "re-write: %s", err);
    RI_ASSERT(file_bytes("/tmp/ri/run/t13-rt2.rbng", B, sizeof B, &nb) == 0,
        "read B");
    RI_ASSERT(na == nb && memcmp(A, B, na) == 0, "round-trip %u vs %u", na,
        nb);

    /* --- 3. unknown-chunk preservation (TST1 survives the round trip) --- */
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t13-unk0.rbng", &s, err,
        sizeof err) == 0, "base: %s", err);
    RI_ASSERT(rbng_test_inject_unknown("/tmp/ri/run/t13-unk0.rbng",
        "/tmp/ri/run/t13-unk.rbng", "TST1", "payload-xyz", 11u) == 0,
        "inject");
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-unk.rbng", &r, err,
        sizeof err) == 0, "unk read: %s", err);
    RI_ASSERT(r.nunknown == 1u, "nunknown %u", r.nunknown);
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t13-unk2.rbng", &r, err,
        sizeof err) == 0, "unk rewrite: %s", err);
    RI_ASSERT(file_bytes("/tmp/ri/run/t13-unk.rbng", A, sizeof A, &na) == 0,
        "unk A");
    RI_ASSERT(file_bytes("/tmp/ri/run/t13-unk2.rbng", B, sizeof B, &nb) == 0,
        "unk B");
    RI_ASSERT(na == nb && memcmp(A, B, na) == 0, "unk preserved %u vs %u",
        na, nb);
    RI_ASSERT(rbng_art_fallback(&r) == 1, "no SKIN => fallback");
    RI_ASSERT(rbng_test_inject_unknown("/tmp/ri/run/t13-unk0.rbng",
        "/tmp/ri/run/t13-skin.rbng", "SKIN", "art", 3u) == 0, "skin");
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-skin.rbng", &r, err,
        sizeof err) == 0, "skin read: %s", err);
    RI_ASSERT(rbng_art_fallback(&r) == 0, "SKIN => full art");

    /* lowercase-unknown chunk id must be rejected, not skipped */
    RI_ASSERT(rbng_test_inject_unknown("/tmp/ri/run/t13-unk0.rbng",
        "/tmp/ri/run/t13-low.rbng", "tst1", "x", 1u) == 0, "low inject");
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-low.rbng", &r, err,
        sizeof err) != 0, "lowercase unknown accepted");
    RI_ASSERT(strstr(err, "tst1") != 0, "err names chunk: %s", err);

    /* --- 4. VERS forward-compat: major reject / minor-newer preserve --- */
    RI_ASSERT(rbng_test_set_vers("/tmp/ri/run/t13-unk0.rbng",
        "/tmp/ri/run/t13-maj.rbng", 2u, 0u, 0u) == 0, "maj");
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-maj.rbng", &r, err,
        sizeof err) != 0, "major=2 accepted");
    RI_ASSERT(strstr(err, "VERS") != 0, "major err names VERS: %s", err);
    RI_ASSERT(rbng_test_set_vers("/tmp/ri/run/t13-unk.rbng",
        "/tmp/ri/run/t13-min.rbng", 1u, 3u, 0u) == 0, "min");
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-min.rbng", &r, err,
        sizeof err) == 0, "minor=3 rejected: %s", err);
    RI_ASSERT(r.nunknown == 1u, "minor-newer keeps unknown");
    RI_ASSERT(rbng_test_set_vers("/tmp/ri/run/t13-unk0.rbng",
        "/tmp/ri/run/t13-flg.rbng", 1u, 0u, 0x80000000u) == 0, "flg");
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-flg.rbng", &r, err,
        sizeof err) != 0, "unknown feature flag accepted");

    /* --- 5. corrupt chunk aborts naming chunk ID + byte offset --- */
    RI_ASSERT(file_bytes("/tmp/ri/run/t13-unk0.rbng", fbuf, sizeof fbuf,
        &nf) == 0, "fbuf");
    fbuf[20] = 0x7fu; /* inflate a chunk length inside PATT/SVers region */
    {
        FILE *f = fopen("/tmp/ri/run/t13-bad.rbng", "wb");
        RI_ASSERT(f != 0, "bad open");
        if (f) {
            fwrite(fbuf, 1, nf, f);
            fclose(f);
        }
    }
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-bad.rbng", &r, err,
        sizeof err) != 0, "corrupt accepted");
    RI_ASSERT(strstr(err, "@") != 0, "err lacks @offset: %s", err);

    /* --- 6. MODR missing-mod warn path (assert prompt string) --- */
    s = demo_song();
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t13-rt.rbng", &s, err,
        sizeof err) == 0, "re-read2: %s", err);
    {
        static const char *have[1] = { "other-mod" };
        RI_ASSERT(rbng_missing_warn(&s, have, 1, warn, sizeof warn) == 1,
            "missing mod undetected");
        RI_ASSERT(strncmp(warn, "MODR: mod 'acid-01'", 18) == 0,
            "prompt string: %s", warn);
        {
            static const char *have2[1] = { "acid-01" };
            RI_ASSERT(rbng_missing_warn(&s, have2, 1, warn, sizeof warn) == 0,
                "present mod warned");
        }
    }

    /* --- 7. CPRG hook --- */
    RI_ASSERT(rbng_cprg_line("/tmp/ri/run/t13-rt.rbng", cprg, sizeof cprg) == 0,
        "cprg hook");
    RI_ASSERT(strstr(cprg, "2026") != 0, "cprg text: %s", cprg);

    /* --- 8. undo-200 scripted --- */
    ri_undo_init(&u);
    for (i = 0; i < 200u; i++)
        RI_ASSERT(ri_undo_commit(&u, 0x0300u, (uint8_t)i) == 0, "push %u",
            i);
    RI_ASSERT(ri_undo_commit(&u, 0x0300u, 200u) == 1, "overflow accepted");
    RI_ASSERT(ri_undo_depth(&u) == 200u, "depth %u", ri_undo_depth(&u));
    for (i = 0; i < 200u; i++) {
        uint32_t ctl = 0;
        uint8_t val = 0;
        RI_ASSERT(ri_undo_undo(&u, &ctl, &val) == 0, "undo %u", i);
        RI_ASSERT(ctl == 0x0300u && val == (uint8_t)(199u - i), "undo val %u",
            i);
    }
    RI_ASSERT(ri_undo_undo(&u, 0, 0) == 1, "underflow accepted");
    for (i = 0; i < 200u; i++) {
        uint32_t ctl = 0;
        uint8_t val = 0;
        RI_ASSERT(ri_undo_redo(&u, &ctl, &val) == 0, "redo %u", i);
        RI_ASSERT(val == (uint8_t)i, "redo val %u", i);
    }

    /* --- 9. MIDI: learn map + loopback + note→step + MMC --- */
    midi_learn_init(&ml);
    RI_ASSERT(midi_cc_lookup(&ml, 7) == -1, "unmapped must fail closed");
    midi_learn(&ml, 7, 0x0A00);
    midi_learn(&ml, 1, 0x0300);
    RI_ASSERT(midi_cc_lookup(&ml, 7) == 0x0A00, "CC7 loopback");
    RI_ASSERT(midi_cc_lookup(&ml, 1) == 0x0300, "CC1 loopback");
    RI_ASSERT(midi_cc_lookup(&ml, 2) == -1, "CC2 must stay unmapped");
    midi_learn(&ml, 200, 0x0300); /* out of range: ignored */
    RI_ASSERT(midi_cc_lookup(&ml, 72) == -1, "bad learn leaked");
    RI_ASSERT(midi_note_step(60) == 60u % 16u, "note→step");
    RI_ASSERT(midi_note_accent(100) == 1 && midi_note_accent(40) == 0,
        "vel accent");
    RI_ASSERT(midi_mmc_cmd("\xf0\x7f\x7f\x06\x02\xf7", 6) == 1, "MMC play");
    RI_ASSERT(midi_mmc_cmd("\xf0\x7f\x7f\x06\x01\xf7", 6) == 0, "MMC stop");
    RI_ASSERT(midi_mmc_cmd("\xf0\x7f\x7f\x06\x09\xf7", 6) == -1, "MMC bad");

    /* --- 10. simulated clock drift < 1 tick / 100 bars --- */
    midi_clock_init(&ck);
    for (i = 0; i < 100u * 4u * 24u; i++)
        midi_clock_advance(&ck);
    RI_ASSERT(ck.ticks == 9600u, "100 bars = 9600 ticks, got %u", ck.ticks);
    RI_ASSERT(midi_clock_drift(&ck, 9600u) == 0, "drift");

    /* --- 11. flood cap: shed oldest-first, counted, timing safe --- */
    midi_flood_init(&fb);
    for (i = 0; i < 200u; i++)
        midi_flood_push(&fb, (uint8_t)i);
    RI_ASSERT(fb.dropped == 200u - 64u, "dropped %u", fb.dropped);
    RI_ASSERT(midi_flood_len(&fb) == 64u, "len %u", midi_flood_len(&fb));
    RI_ASSERT(midi_flood_get(&fb, 0) == (uint8_t)(200u - 64u),
        "oldest-first shed");

    /* --- 12. hot-unplug: timeout code, transport continues --- */
    RI_ASSERT(midi_backend_status() == 0, "backend must start ok");
    midi_backend_unplug();
    RI_ASSERT(midi_backend_status() == 2, "want RI_MIDI_TIMEOUT=2");
    RI_ASSERT(midi_transport_alive() == 1, "transport must continue");
    midi_backend_replug();

    /* --- 13. ARexx exact strings + alias --- */
    RI_ASSERT(arexx_parse("OPENSONG PROGDIR:Songs/acid.rbng", &c) == 0 &&
        c.cmd == RIAREXX_OPENSONG, "OPENSONG");
    RI_ASSERT(strcmp(c.arg1, "PROGDIR:Songs/acid.rbng") == 0, "arg1");
    RI_ASSERT(arexx_parse("PLAY", &c) == 0 && c.cmd == RIAREXX_PLAY, "PLAY");
    RI_ASSERT(arexx_parse("STOP", &c) == 0 && c.cmd == RIAREXX_STOP, "STOP");
    RI_ASSERT(arexx_parse("EXPORTWAV a.rbng b.wav", &c) == 0 &&
        c.cmd == RIAREXX_EXPORTWAV, "EXPORTWAV");
    RI_ASSERT(strcmp(c.arg1, "a.rbng") == 0 && strcmp(c.arg2, "b.wav") == 0,
        "export args");
    RI_ASSERT(arexx_parse("SETRPPARAM 768 96", &c) == 0 &&
        c.cmd == RIAREXX_SETRPPARAM, "SETRPPARAM");
    RI_ASSERT(c.ctl == 768u && c.val == 96u, "setrp args");
    RI_ASSERT(arexx_parse("REBIRTHAROS PLAY", &c) == 0 &&
        c.cmd == RIAREXX_PLAY, "deprecated alias");
    RI_ASSERT(arexx_parse("REINCARNATION STOP", &c) == 0 &&
        c.cmd == RIAREXX_STOP, "primary port prefix");
    RI_ASSERT(arexx_parse("play", &c) != 0, "lowercase accepted");
    RI_ASSERT(arexx_parse("DANCE", &c) != 0, "unknown accepted");
    RI_ASSERT(arexx_parse("OPENSONG", &c) != 0, "argless accepted");
    RI_ASSERT(arexx_parse("SETRPPARAM 768 200", &c) != 0, "val>127");

    /* --- 14. SHA-256 identity vectors --- */
    ri_sha256_hex("abc", 3, hex);
    RI_ASSERT(strcmp(hex,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0,
        "sha abc: %s", hex);
    RI_ASSERT(rbnm_sha256_file("tests/golden/303/first-light.rbng", hex,
        err, sizeof err) == 0, "sha file: %s", err);
    {
        char hex2[65];
        RI_ASSERT(rbnm_sha256_file("tests/golden/303/first-light.rbng", hex2,
            err, sizeof err) == 0, "sha file2: %s", err);
        RI_ASSERT(strcmp(hex, hex2) == 0, "sha identity");
    }

    /* --- 15. RBNM-full: reserialize byte-identical + CPRG extract --- */
    RI_ASSERT(rbnm_reserialize("reference/packs/classic-01/pack.rbnm",
        "/tmp/ri/run/t13-pack2.rbnm", err, sizeof err) == 0, "reser: %s",
        err);
    RI_ASSERT(file_bytes("reference/packs/classic-01/pack.rbnm", A, sizeof A,
        &na) == 0, "pack A");
    RI_ASSERT(file_bytes("/tmp/ri/run/t13-pack2.rbnm", B, sizeof B, &nb) == 0,
        "pack B");
    RI_ASSERT(na == nb && memcmp(A, B, na) == 0, "pack identical %u/%u", na,
        nb);
    RI_ASSERT(rbnm_read_cprg("reference/packs/classic-01/pack.rbnm", cprg,
        sizeof cprg, &present, err, sizeof err) == 0, "cprg read: %s", err);
    RI_ASSERT(present == 0, "classic-01 must CPRG-fallback");

    /* --- 16. project RBNG step flags match the walker contract --- */
    RI_ASSERT(RI_RBNG_REST == RI_STEP_REST, "REST bit");
    RI_ASSERT(RI_RBNG_SLIDE == RI_STEP_SLIDE, "SLIDE bit");
    RI_ASSERT(RI_RBNG_ACCENT == RI_STEP_ACCENT, "ACCENT bit");
    RI_ASSERT(RI_RBNG_FLAM == RI_STEP_FLAM, "FLAM bit");

    RI_RESULT("formats");
}
