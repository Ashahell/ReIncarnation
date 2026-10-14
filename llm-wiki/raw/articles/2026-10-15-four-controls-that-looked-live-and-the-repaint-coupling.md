# Four controls that looked live and were not, and the repaint coupling that finally closed the class (2026-10-15)

- Source: ReIncarnation session (OpenCode lane), 2026-10-14/15. Range `e846eff..8be9d80`; ear-proofed at `f087b82`.
- Evidence: `tests/unit/t222_fx_exclusive.c`, `t223_fx_lamp_repaint.c`, `t224_decorative_sweep.c`, `t225_strip_mute.c`, `t226_delay_fx_diag.c`, `t227_mix_insert_lamp.c`, `t228_press_repaint.c`, `t229_mute_join.c`; `tests/mutate_fx_exclusive.py` (7/7), `tests/mutate_fx_lamp.py` (6/6); `docs/evidence/midi/ledger.md` §§P-FX3, P-FX4, P-DEC1, P-MIX1, P-MIX2, P-RPN1, P-MIX3, and the ear-proof closeout.
- Collected: 2026-10-15
- Published: 2026-10-15
- Spec: `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (§12.8 insert routing, §13 FX)
- Prior: the FX enable half is the sibling's arc — `2026-10-15-fx-meter-closeout-comp-gr-bar-and-frozen-displaced-meters.md` and `docs/evidence/midi/ledger.md` P-FX1/P-FX2; `2026-09-25-gui-parity-sections-g1-g4.md` line 36 (see the Status note added there)

## The shape

Five fixes across two days, and **four of them were one defect in different clothes**: a control that a user could see, click, and read as live, and which did nothing.

| | what was missing |
|---|---|
| P-FX1 (sibling's arc) | an engine target for the FX enable |
| P-FX4 | a repaint for the displaced FX lamp |
| P-MIX1 | an engine target for the strip mute — the concept did not exist |
| P-MIX2 | a repaint for the displaced insert rocker |
| P-MIX3 | agreement on which way round the value goes |

The first two were **not bound at all**; the second pair were **correct in state and never repainted**. Those are different bugs that look identical to a user, which is why each had to be found separately before the class could be named.

## The structural cause, measured twice

Two facts, both verified in code rather than assumed:

1. `gui/widgets/rsection.mcc.c` contains **no `Invalidate` and no self-redraw**. Every repaint is an explicit `ri_rsection_refresh()` from the shell.
2. `sync_values()` — the per-frame loop that runs on **every** panel change — **pushed values and repainted nothing**.

And the platform detail that turns "eventually right" into "frozen": on AROS Zune a draw request is honoured **1:1**. `rsection.mcc.c` records this explicitly (`workbench/libs/muimaster/mui_redraw.c` calls `MUIM_Draw` inline, with no deferral), after an earlier comment in the same file had assumed MUI coalesced redraws and been wrong about it.

So an unrefreshed control is not eventually correct — it keeps whatever pixels were last drawn until *unrelated* code redraws its canvas. That is exactly why the mixer insert rockers appeared to light up: the mixer meter loop redraws the whole canvas whenever a strip level changes, and the rocker picked up its value as a side effect. Press again, nothing refreshes, and the lamp freezes.

## `t116` stayed green through an inverted switch

The most transferable finding. `t116_panel_wiring` round-trips panel → bridge → drain → apply and compares against a `direct()` that is a **reference mirror** of the engine's dispatch. When the strip On/Off was inverted, **both sides were inverted identically**, so they agreed perfectly and the gate confirmed nothing. Its own comment says it *"proves the panel path and the direct path agree, not that either is right"* — a caveat that had cost nothing until it hid a sign error for a full build.

**A test that compares two implementations of the same rule cannot detect a wrong rule. Only one that asserts on the output can.** `t229` replaces it for this control: it drives the panel value through the real control plane (`registry id → ri_panel_ctl_send → drain → apply`, the exact path `sync_values` uses) and asserts on what leaves the **renderer** — all five squares at `0` is bit-identical to neutral, all five at `1` is audible, one *non-sounding* strip off is still audible, the *sounding* strip off is silent. The last pair is what catches a fix that merely inverts the whole array.

## Naming, because the name was half the bug

`ri_engine_set_mute` → `ri_engine_set_enable`, `e->mute[]` → `e->enable[]` defaulting to **1**. The panel row is a switch with `def_v = 1` whose lamp reads `val != 0`, so **1 means on and audible**. The first implementation stored the panel value verbatim into a flag where 1 meant *muted*, and `sync_values` pushes verbatim, so a lit square silenced the strip. The flag is now named for what the **panel** means by the value, because the panel is what feeds it; the render guard reads `!e->enable[section]` and its comment says it is *"tested by the SIGN, not the name"*, so the next reader cannot flip it by matching the identifier.

## The sweep, and an invariant that had to be thrown away

`t224_decorative_sweep` looks for the shape: an interactive control with hit-testable geometry that cannot leave the panel. It found **all five strip mutes** (`0400 0500 0600 0700 1400`) with no prior knowledge of which sections they lived in — the ear had found one instance by hand.

Its first run flagged **135 rows** and only ~5 were real. The rest are entirely functional controls that act on **panel state** rather than on the engine: pattern Step 1–16 and Pitch C–C+ buttons write steps, Transport Play/Stop/Record/Tap drive the shell, Bank/Pattern/Shuffle are panel selection. All have `auto_id == 0`; all do real work. **"no engine auto_id ⇒ decorative" is false.** Gating on it would have failed the audit 135 times and taught everyone to ignore the gate, so `t224` **reports** its census instead of asserting it and says why in its header. The missing half is owner knowledge — what each control is *supposed* to do — which a sweep cannot infer.

## The coupling, and why the cross-canvas half stays

P-FX4 and P-MIX2 each patched one more call site. The site that should have covered both is `sync_values`, which sees every change: it now repaints the box of whatever changed, replacing the mixer-only block P-MIX2 had added (`514f4ee`). The one decision worth testing is **which** controls get one, and that is portable — `ri_ctl_press_repaints()`:

- **METER rows: no.** The meter loops already refresh those boxes whenever the *level* changes — every frame, not on a press. Including them would double the invalidations for no visual gain, and this widget counts redundant invalidations (`dpr_rep`) because they were implicated in a **1.36 s stall**.
- **Everything else: yes**, including DISPLAYs whose arrows change the number.
- The predicate takes only the *definition*, never the *value*, so a knob **drag** — which fires the change counter continuously — is handled like a click.

`t228` censors **every** METER row in the registry rather than the three named, so a meter added later cannot begin double-invalidating unnoticed; RED caught 12 leaked rows against the stub.

Two categories remain, and the second cannot be folded into the first:

| trigger | mechanism |
|---|---|
| own value changed (a change event here) | `sync_values`, general |
| changed by **another's** press (**no** event here) | cross-canvas bookkeeping: `ri_sfx_rack_stale_section()`, `ri_smix_stale_section()` |

The second is not a workaround. The displaced canvas genuinely receives nothing, so it must be told.

## The trap inside the mixer fix

A route owner is a **number** (0–4 plus master); a lamp lives on a **section**; and the two numberings are not the same — strip 4 is the master strip, yet route section 4 is the Levi (`sectmix.c` says so in a comment that already existed). Inferring the inverse would repaint the **wrong canvas**, which is the failure this change exists to remove, so `section_of_owner()` is explicit and `t227` pins each mapping.

## Status

Ear-proofed at `f087b82`: *"ok, that all seems to work correctly now."* The load-bearing item is the strip On/Off, which the owner had reported failing by ear on the previous build (*"turn off all 5 green squares ... we still hear all sounds"*) and which was directly, not inferentially, observed to change.

**Unproven:** FX engaged by MIDI or song automation still moves no lamp — the same readback gap as the COMP GR meter, deliberately unsolved. **Unmeasured:** P-RPN1 adds a draw request per control change on every canvas; nothing regressed visibly, but nobody has read `dpr_rep` / `dpr_run_max` on the lane.
