# Keybed plays notes, and STEP RECORD gates the step write (2026-10-15)

- Source: ReIncarnation session (OpenCode lane), 2026-10-14. Commits `017e5e7` (part 1) and `2b52ffc` (part 2), tag `[levi-midi]`.
- Evidence: `tests/unit/t221_keybed.c`, `t107_levi_sect.c` (both states of the gate), `tests/mutate_keybed.py` (5/5); `docs/evidence/midi/ledger.md` §P-KEY1.
- Collected: 2026-10-15
- Published: 2026-10-15
- Spec: `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (§13 panel, §12 keybed)
- Prior: none — this closes P-KEY1, opened in `2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md`

## Owner spec

> "the keys play notes, through the Leviasynth note path MIDI input already uses … They write the pitch into the selected step only while STEP RECORD is lit (manual: Step Recording p. 105, step record mode p. 117). That keeps today's step entry, just behind the button the hardware uses for it. Velocity is fixed (an E0 default, 100)."

## Turning a level back into two edges

`RICevState.key_id` (part 1, `017e5e7`) is unreachable from the shell, so it is mirrored into `RSectionDiag.key_held` — the same route `last_hit` already takes, rather than a new MUIA attribute. The shell then compares what it last **sent** against what is **held**: a key that appears is note-on, a key that disappears is note-off.

```c
ri_ctl_send2(ctl, RI_CTL_LEVI_NOTE, RI_LEVI_KEY_VEL, 0x80u | note);  /* on  */
ri_ctl_send2(ctl, RI_CTL_LEVI_NOTE, 0u, note);                        /* off */
```

That is the exact call the MIDI bridge uses, so the note travels the path the
ear proof had already witnessed. Velocity is `RI_LEVI_KEY_VEL 100`, the owner's
E0 default, named in the header **with the reason** (the hardware keybed is not
velocity-sensitive and the owner ruled a fixed value over an invented curve).

## STEP RECORD already existed, and the method that found out

Searching for a STEP RECORD control and finding none, `RI_SLEVI_STEPREC 228` was added with a registry row, geometry and `RI_SLEVI_NCTL 229` — and then **four separate censors were bumped to absorb it**. `t60_ctlreg` caught the real fault: *"key 0eb5 shared by rows 410 and 425"*. `R(LEVI, 204, SWITCH, "Seq", "Rec", …)` has driven `RI_CTL_LEVI_SEQREC` since P7a; it is labelled **"Rec"** in the "Seq" group, which is why a search for "STEP RECORD" never touched it.

The genuine gap was never a missing control: **204 had no geometry row**, so the button was a painted label and unhittable. Duplicate deleted, `NCTL` back to 228, geometry moved onto the existing row, all four census edits reverted. Net change to the registry: **one geometry row, zero new rows.**

Two lessons: **search by what a control drives, not by its label**; and a cascade of censors that all move together is a signal that the change is wrong, not that the tests are stale.

## Status

`AUDIT 0/0 PASS`, mutation 5/5, AROS build clean.

**Never heard.** Implemented on unit tests and a clean build only. The lane log gained `LEVI key N on/off` lines, so the next session can confirm the press latched and the note went out before anyone goes looking for the sound. This is the one piece of this session's work with **no ear evidence at all**, and it is older than everything else here.

## Status addendum (2026-10-23, P-KEY2 `d9f2af3`)

Two dated notes, each left standing where it was written rather than rewritten:

- The owner spec's **"(an E0 default, 100)"** and `RI_LEVI_KEY_VEL 100` — **reversed on
  the owner's say-so 2026-10-23**: the ear round heard the keys and called the volume low,
  and the fixed value is now **127**, the maximum the note path takes. **Fixedness is
  unchanged** (the hardware keybed is not velocity-sensitive); only the value moved, and
  `t238_levi_key_paint` pins the engine's terms (`vel01 == 1.0f`, not 100/127).
- The **edge layer here was correct, and the paint under it was not**: on 2026-10-23 an
  owner round logged 18 presses and **every one a white-key note**, because live whites
  painted the whole keybed band over the black keys, so a 36 Q black survived at about
  4 Q and the full-width black keys the owner aimed at were art decoration with no control
  behind them (22 art whites against 13 live keys). See
  `raw/articles/2026-10-23-the-keybed-painted-slots-it-does-not-own.md`.

**The "Never heard" line still stands** as of this addendum: the fix commit's build
(1237704 bytes, sha256 `27b5879dfa…`) has not run on the Dell yet, so the keybed remains
the one piece of this range with no ear evidence.
