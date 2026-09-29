# Leviasynth v2 slices: look verdicts, arp, seq, matrix, LFO, reverb (owner 2026-09-29)

- Source: ReIncarnation session, 2026-09-29 (opencode lane; owner verdicts in chat; sibling GUI-round-3 session concurrent in the same tree)
- Collected: 2026-09-29
- Published: 2026-09-29
- Requirement + E1: [2026-09-28-planned-device-asm-leviasynth.md](2026-09-28-planned-device-asm-leviasynth.md)
- Prior: [2026-09-28-leviasynth-v1-implementation.md](2026-09-28-leviasynth-v1-implementation.md)
- Coordination record (not in repo): `/tmp/opencode/coord-arp-s4.md`

## Owner verdicts (chat 2026-09-29)

- Levi look CLOSE but not good enough: piano section missing, knobs not vertically aligned, Pattern-Levi absent on the real synth. Fixed in `fba990e`, then **approved** (look + sound) on the Dell.
- Dell rebooted (no sound); build `fba990e` deployed fresh, hash-verified in `RAM:RIAPP`, single instance. Owner drove it live: PLAY → Levi tab (`TAB page=2`) → step edit → ~385 knob twists → STOP, ev-log ink, no guru.
- Feature order stands (requirement §v2): arp+seq, then matrix, then FX. Matrix slot-editor panel needs a panel-room verdict (MATRIX band full); phrase-looper semantic chosen over a full 3-track store (no panel surgery).
- USB0 volume requester on the Dell is NOT lane traffic (lane jobs touch only `RAM:RIAPP`, `Vk4aros:RIAPP-EV.LOG`, captures, status).

## Look fixes (`fba990e`: knob row, keyboard section, voice-only tab)

- Top-band knobs share `cy=130`, legends above at `y=70` (was 115/125 mixed, legends scattered 90/160). Pinned in t61 (knob-row + key-row asserts).
- Piano is now a labeled KEYBOARD section (taller whites `cy=380 h=90`, blacks `cy=335 h=60`, title bar + block in `art_levi`).
- Levi tab is voice-only (`RI_TAB_PAT_NONE`): the real instrument has no pattern section; `C_P4`/`sync_pat` engine keeps running slot 0 underneath. Pinned in t98.
- Mutants kill (knob-row, pat-row; second needed a libs rebuild first — stale `$OUT/*.o` false-passes, standing trap).
- t92/t93 sec-18 re-pinned deliberately, all other sections bit-identical. Host render verified.

## Arp, v2 feature 3 first half (`8bf6c76`, `8000cca`, `96c332a`, `a71b47e`, `19578fa`)

- 3a stepper core (`8bf6c76`, t113): own 8 modes (up/down/updown/chord/octup/octdown/random/entropy); Entropy wanders a struct-held LCG seeded at start (no RNG/globals, `levi.h` determinism law); rate map 0..127 → 0/1/2/4 steps per quarter (delay-tempo-clock precedent).
- 3b-i emit rewrite (`8000cca`, t114): post-pass over a Levi event window; off/sub-audible rate copies bit-identically (demo safety); on subdivides per STEPSQ with legato voice rotation and balanced gates; seed = group sample. Tail + single-group density defects caught by t114 during development and fixed (step grid param, exclusive-end strikes, tail at end-1).
- 3b-ii/1 binding (`96c332a`): `0x0E11/12` allowed+bound, device truth on `RILeviSet`, ARPON MODE-style toggle; ARP block wakes visually (Rate knob leaves `C_DISABLED`), verified on host render, sec-18 re-pinned. Boundary pins flipped deliberately (t77/t106/t107/t92/t93).
- 3b-ii/2 player hook (`a71b47e`, t115): player owns the arp cfg (default off); occurrence rewrites post-emit with the real step grid. `riapp.c` needs NO change (`sync_values` generic — verified, not assumed).
- t80 updated to the allow move (`19578fa`).
- OPEN: engine→player truth copy (~4 lines in sibling `engine.c`, specced in the coord note). Until then arp idles off, demo bit-identical.

## Seq, v2 feature 3 second half (`8b2bd48`, `a34f6be`, `cc1fa35`)

- 3c-i phrase window (`8b2bd48`, t117): pure pre-pass `out[i] = in[i % N]` (truncation + wrap-extension, N 1..16, fail-closed). T-number deconflicted (sibling owns `t116_panel_wiring`).
- 3c-ii hook + binding (`a34f6be`): player cfg, occurrence pre-pass over a struct copy, `0x0E13/14` allowed+bound, SEQON toggle; SEQ block wakes visually, sec-18 re-pinned.
- `cc1fa35`: t60 id-range follow-up.
- t118 needed its gate-balance assert replaced: block-sliced retrigger streams legitimately show N/0 per voice (release belongs to the next block, v1 same) — stream order + subdivision + determinism asserted instead; rewrite/direct balance stays pinned in t114/t102.

## Matrix, v2 feature 4 (`e2755f4`, `f3383c9`, `99c87a6`, `d7288f9`)

- 4a slot core (`e2755f4`, t119): 32 slots (src/dst/depth/gate) + pure eval summing normalized offsets. Sources from today's signals only: 8 op contours (spec: contours as sources) + keytrack; ids 8..15 reserved.
- 4b render hook (`f3383c9`, t120): per-sample eval in `levi_voice_render` (new matrix arg; call-site-only updates to t103/t108/t109 whose expectations stand unchanged — v1 sound law). Own scaling: cutoff ±2 oct, reso/drive linear, morph blend units, oplevel pre-filter, vlevel post. Empty program bit-identical; voice truth never written.
- 4c-A route gates (`99c87a6`): `0x0E15..1C` gate slots 0..7 end to end. RECT switches render bind-blind (verified in `art_section`), so zero visual change — t92/t93 unmodified, no look verdict.
- 4d LFO core (`d7288f9`, t122): 5 per voice, 0.01..30 Hz exp (100 s/cycle floor, spec), smooth sine or quantized 3-step `{-1,0,+1}` (own reading), trigger-reset phases. Sources 8..12 live. CTL IDs `0x0E1D..26` accepted engine-internally, deliberately NOT allow-listed (no panel yet — automation binds with the 4c-B editor; pinned in t106 so nobody "fixes" it).
- Keytrack law corrected mid-slice: `(n-60)/72` put full-scale at note 132 (outside MIDI) → `(n-60)/60`, full-scale on C8.
- OPEN: 4c-B slot editor (needs panel-room verdict), LFO automation, route-switch visual stays dim until programmed.

## Reverb, v2 feature 5 started (`44f1c2e`)

- 5a core: own 4-comb + 2-allpass network (own lengths 1123/1201/1291/1361 + 401/271, own gains), wet-only, decay-0 exact silence past the longest tap, decaying tails, twin determinism, 10 s finite noise, reset. First mutant drafts were unobservable (silence window shorter than the longest tap; allpass recirculation sustains decay) — replaced with a long-silence law and runaway gain; both kill.
- 5b send integration SPECED for sibling `engine.c/h` (delay-send mirror) + t124 drafted in `/tmp` (uncompilable until the API exists, NOT committed). AROS TU + `portable.mk` registration rides with 5b.

## Coordination record (shared tree, no message channel)

- `session_inbox` is harness-internal steer traffic, not a mailbox; `/tmp/opencode/coord-arp-s4.md` is the channel (owner relays).
- Rule: hunk-inspection before every commit; sibling set (engine.c/h, live.c/h, ctlplane, panelctl, riapp.c, build lists, art, pins, wiki, zoomfit) never touched. Shared-infra files: audit file hunk-split per-hunk, host build line committed whole with note (panelgeo precedent).
- Sibling commit messages independently confirm the shared gates (their S4 notes the t80/0x0E11 breakage from this lane; fixed here).

## Methodology findings (portable)

- Stale-object false PASS: a mutant that breaks the build leaves the old `.o`; the test links it and passes. Always hash-compare objects across mutant rebuilds (one false-pass caught live this round).
- Vacuous asserts: a mutant writing slot[0] for all routes passed because the test never set slot[7] nonzero first. Set-then-clear with neighbor checks.
- Approx kernels vs exact equality: `ri_pow2` is approximate — assert ranges (29,30], never `== 30.0f`. (Exact `pow2(0)==1` proven by the empty-matrix identity test.)
- MIDI-domain laws: full-scale must land on a real note (keytrack, LFO ceiling).
- Test-number registry is now shared: sibling owns t116/t121 (+t112, t115_master_live); this lane owns t113–t115, t117–t120, t122–t123.

## Open

- Sibling: `t75` bg03 art (909 widening orphaned the 1460-wide `BACKGROUND.909`), engine→player arp/seq truth copy, reverb-send block.
- Owner: 4c-B slot-editor panel-room verdict; full 3-track store vs phrase looper (looper chosen); device proof lane for arp/seq/matrix/LFO once runtime links land; RBNG chunk IDs; MIDI/focus-5 slice needs the interop spec (`b2c35ee`) re-read at scoping time.
