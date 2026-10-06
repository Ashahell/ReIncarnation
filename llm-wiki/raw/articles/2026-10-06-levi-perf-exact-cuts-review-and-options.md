# Levi performance: a bit-exact oracle, five exact cuts (Dell lev-voice −29 %), one latent bug found in review, and two sound-changing options the owner turned on (2026-10-06)

- Source: ReIncarnation session (advisor lane) and the opencode lane, 2026-10-06. Opencode commits `4529579`..`e6d96b2` (levi-perf P0–P4), advisor review fix `e125db8`, dispatch prompts `4bcc9e8` and `docs/superpowers/plans/2026-10-06-levi-options-enable-opencode-prompt.md`.
- Evidence (verbatim tool output): `docs/evidence/levi-perf/2026-10-06-p0-oracle.md`, `…-p1-cost-model.md`, `…-p2-exact-cuts.md`, `…-p3-options.md`, `…-p4-dell.md`
- Collected: 2026-10-06
- Published: 2026-10-06
- Related: [2026-10-04-host-bench-and-the-overload-guard.md](2026-10-04-host-bench-and-the-overload-guard.md) (whose two "no prize" conclusions this record overturns), [2026-10-03-on-the-dell-levi-is-64-percent-of-the-block-and-91-percent-is-one-call.md](2026-10-03-on-the-dell-levi-is-64-percent-of-the-block-and-91-percent-is-one-call.md), [2026-10-04-the-lfo-path-never-runs.md](2026-10-04-the-lfo-path-never-runs.md), [2026-10-05-review-latency-instrumentation-and-advice-log.md](2026-10-05-review-latency-instrumentation-and-advice-log.md)

## The owner's request

"Leviasynth performance": measure by voice count, operator count and Morph
on/off; make the cuts that leave the sound unchanged first; bring
sound-changing options with measured savings. "Unchanged" was defined as
**bit-exact**: the float output of `levi_voice_render_sum_stereo`, compared as
`uint32_t` bit patterns, over a scripted corpus.

## The two 2026-10-04 bench conclusions were artefacts

- **The operator sweep measured nothing.** It wrote `slot[o] = RI_LEVI_SLOT_SILENCE`,
  but slots take effect only through `morph_apply()` → `bank_load()`, which
  the bench never called. `live[]` was unchanged and all 8 operators kept
  rendering. Opencode confirmed this with a counter (P1 G3). The real
  per-operator slope on an 8-op stack is 0.0224 µs per voice-sample.
- **The morph sweep could not see the bank-skip prize.** Both banks'
  `voice_pass` run unconditionally, so morph 0/50/100 do identical work. The
  flat line was expected; it was not evidence that skipping has no prize.
  - Priced directly, bank B's pass is **19–24 %** of the voice render: "delta
    0.1261 = 24.2% of the 16-op patch", and 19.1 % on the worst patch.
- **The 2026-10-04 "LFO path never runs" counter** saw only the shared
  (global) LFOs. Every voice is rendered with a non-NULL matrix pointer, so
  each voice steps its own 5 LFOs and runs `levi_mod_apply` on every sample,
  even with an empty matrix (H5 confirmed).

**Lesson: a sweep row without a positive control (a counter proving the swept
quantity changed) is not a result.** The 2026-10-04 bench was carefully timed
and still measured the wrong thing twice.

## P0: the oracle

- `t172_levi_bitexact` (gated) covers 21 cases, each rendered in 64- and
  256-sample blocks. Hashes are FNV-1a 64 over the sample bit patterns.
- **Pins:** generated on `ebc94ec`, before any engine change.
- **Coverage:** algorithms, all waves, op modes, filters, morph held and moving,
  matrix families, LFOs, mod envelopes, pans, vintage, pitch, performance
  controls, FX, tails, a 44.1 kHz rate change, and (added in review) a mono
  render between stereo blocks.
- **Mutation proof:** the naive "skip bank B when morph is 0" mutant fails on
  the moving-morph case. This proves the oracle sees state, not just output.
- **Review check:** I ran the current t172 against the `ebc94ec` engine
  objects and every pin passes. The corpus cases added during P2 were
  therefore pinned on the original engine, not on optimised code.

## P1: where the time goes (host, `-O2`, worst patch, 0.8758 µs per voice-sample)

| Share | Item |
|---|---|
| 33.2 % | Matrix with 8 routes (eval2, 6 pow2, tpt_g misses) |
| 20.5 % | Bank-A operator bodies |
| 19.1 % | Bank-B `voice_pass` |
| ~6 % | Filter cores |

- `ri_pow2` always ends in `ri_scale2`'s fixed 32-iteration loop (H3).
- 8 of 29 sines per voice-sample sit in `tpt_g` (H2). Its memo would hit
  1.00 on every row.
- The gprof call counts matched the counters exactly
  (`ri_pow2_impl 12902430 calls (== counter 6/vs, exact match)`).

## P2: five exact cuts (t172 pins and song hashes unchanged throughout)

| Cut | What | Measured (host µs per voice-sample, examples) |
|---|---|---|
| C3 | Empty-matrix fast path in `levi_mod_apply`. It reproduces every side effect of the full path: LFO resets, stale-route clears, `melmod`, and the `e*` clamps. A cached `mx_empty` is refreshed by every matrix setter. | worst-nomx −8.6 %, pan centred −15.5 % |
| C1 | `tpt_g` memo per filter instance, keyed on the full `(fc, sr)` bit patterns | pan centred −19.8 %, dfilt VOWEL −27.5 % |
| C2 | Compute the sine only on the wave branches that use it | waves saw ×8 −23.1 % |
| C5 | Dual-mono filter collapse: when `mixL == mixR` bitwise and the L/R states are bitwise equal, run one chain and copy it. Spread voices skip the check. | worst −2.1 % |
| C6 | Skip bank B's pass when it has no live operator (SILENCE bank) | small; P2 stopped here under the < 2 % rule |

- **Dell, ABIv11, A,B,B,A protocol** (A = `dd31d19`, B = `4421b9a`), xruns 0 in
  every cell:
  - Zombie Nation `lev-voice` 236 → 168 µs, "B/A = 0.712 (-28.8%)";
  - The Knife 378/377 → 294/277 µs.
- **Why the songs gain more than the host's worst patch (−3 %):** the songs'
  Levi content is mostly unrouted and centred, which is exactly where C3 and C5
  pay.

## The review found a latent bug in C5 (`e125db8`)

- **The bug:**
  - The mono render `levi_voice_render` advances only the L filter states.
  - A dual-mono lock left set by an earlier centred stereo sample then let the
    next stereo sample copy L over R, so R diverged from the original engine.
- **Proof:**
  - New t172 case c20 "mono-stereo" (stereo, one mono block, stereo, release),
    pinned on the `ebc94ec` objects.
  - RED on `4bcc9e8`: `case mono-stereo blk64: got 9a16c24a37a655b9 want dd6b36ebb3209668`.
  - GREEN with the lock cleared at the mono entry.
  - Audit 0/0 PASS.
- **Reach:** nothing in the app calls the mono sum today, so the bug was
  latent.
- **Rule:** any per-voice cache must be invalidated on **both** render entries.

The review also traced every writer of the new caches. Matrix setters
(including the ROUTE gate in `levi_set_param_ui`) refresh `mx_empty`.
`filt_clear` is the only non-render writer of `df`/`dfR`. `bank_load` is the
only writer of `liveB`. A whole-set `memset` zero-initialises the caches to
safe values.

## P3: sound-changing options, priced (all default-off in `a284347`)

| Option | Saving (host) | Deviation | Owner, 2026-10-06 |
|---|---|---|---|
| O1 control rate (matrix, pitch and filter targets every N samples, interpolated) | about −17 % routed at every N | Zombie N=8 0.050 abs / −71.7 dBFS (−55.7 rel); static patches identical | **ON, N = 8** |
| O2 bank skip at the morph endpoints | −30.4 % at morph 0 | 0 on demo, Zombie and Knife; 0.9258 abs on an LFO → ALGO probe | **ON** |
| O3 float kernels | −12 to −17 % | Knife −77.2 dBFS (−59.0 rel) | not taken; code to be removed |
| O4 voice cap | cap 6 −25 % block | 0 on songs (they bypass the allocator) | not taken; code to be removed |
| O5 FTZ/DAZ | — | zero subnormals measured anywhere | no action |

## Three prototype defects the advisor found before enabling O1 and O2

These are written into the enable prompt as required fixes.

1. **`ctl_init` is never reset on a new note.** Re-triggered or stolen voices
   interpolate pitch and cutoffs from the previous note's control targets for
   up to N − 1 samples, which is audible as a note-start smear.
2. **O2 clicks on LFO-driven morph.** Fix: skip only when no route targets
   `RI_LEVI_DM_ALGO` and morph has sat at the endpoint for at least one
   64-sample block. The LFO probe must then be bit-exact.
3. **O1 holds the gain-like values** (`evlevel`, `eoplevel`, `edlevel`) as
   8-sample steps, which is a zipper on fast amplitude modulation. Fix:
   interpolate them like `dc`/`ac`/`vpitch`.

A smaller fix: `ctl_k % N` is wrap-safe only when N is a power of two, so N
gets a compile-time check.

**After enabling, the exactness rule narrows:** static cases (no modulation
motion, no morph movement) must keep their `ebc94ec`-era pins, and moving cases
are re-pinned deliberately, each with its measured deviation.

## Deploys and lanes

- **Dell stick:** `Vk4aros:ReIncarnation/RIAPP` = `e125db8` (ABIv11 mixed,
  1057296 B, read back and md5-verified); `RIAPP.prev` = `4421b9a`.
- **Sound on the Dell:** the owner hears Zombie Nation on the Dell, which is
  now the listening lane; QEMU ABIv1 has no sound.
- **Agent trap:** an `--ui-windows` job got `ConnectionResetError` and the agent
  came back as a new session. Check for a running RIAPP with `status` instead.

## Process note

The advisor's earlier "commit and push" of the prompt (`4bcc9e8`) also pushed
opencode's eight unreviewed levi-perf commits that sat below it on `main`. The
review came after the push. Before pushing your own commit, check
`git log origin/main..HEAD`.
