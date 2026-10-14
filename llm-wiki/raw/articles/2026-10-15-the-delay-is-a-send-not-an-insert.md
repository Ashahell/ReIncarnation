# The delay is a send, not an insert: two sessions conflated, and the diagnostic that stops it happening again (2026-10-15)

- Source: ReIncarnation session (OpenCode lane), 2026-10-14/15. Range `e846eff..8be9d80`; delay ear-confirmed at `3605381`/`f087b82`.
- Evidence: `tests/unit/t226_delay_fx_diag.c` (five hops + the lane line), `t52_engine_fx.c` (pre-existing send→audible assertion), `app/core/riapp_core.c` `ri_core_delay_diag()`; lane logs preserved at `docs/evidence/midi/dell8-pfx4-earproof.log` and `dell11-pfx4-delay-still-silent.log`; `docs/evidence/midi/ledger.md` §§P-FX4, P-DLY1.
- Collected: 2026-10-15
- Published: 2026-10-15
- Spec: `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (§12.8 insert routing; mixer sends vs inserts)
- Prior: `2026-10-15-fx-meter-closeout-comp-gr-bar-and-frozen-displaced-meters.md` (the FX meter arc this shares a canvas with); `docs/evidence/midi/ledger.md` P-FX3/P-FX4

## What was reported

> "Delay seems to do nothing. It's meter doesn't work, turning knobs makes no audible difference."

Then, after the mixer was reached:

> "Delay in the Mixer works audibly, in FX it doesn't."

## It is a send, and that is not a defect

The delay is a **send**, not an insert, and the topology is **confirmed rather than assumed**: `docs/evidence/gui/panel-geometry.md:95` documents insert routing as PCF / Comp / Dist and **does not list Delay**, and `gui/sectfx.h` states *"which section feeds a unit is set in the mixers."*

The render path is correct and the silence is arithmetic:

```c
sendbus[i] += (float)(s * sg);        /* engine.c:226 */
t = (double)e->send[section] / 127.0; sg = t * t;   /* square law, 0 -> exactly 0 */
R(MIX_LEVI, 4, KNOB, "", "Delay", 0, 127, 0, 111, 1, SEND, 0, 4),   /* def_v = 0 */
```

Every strip's delay send defaults to 0, so `sg == 0`, so `sendbus` is silence, so `retbus` is silence. One fact, both symptoms: the knobs change nothing *because no signal enters the line*, and the meter — fed from `retbus` — reads zero. `e->dline` **is** attached (`c->dline[RI_CORE_DLINE]` is an inline array member), so this is not an allocation fault.

`t52_engine_fx` already pinned the converse, and the audit passed on it the whole time:

```c
RI_ASSERT(ri_engine_set_send(&e, 0, 100) == 0, "send0 up");
RI_ASSERT(differ(LA, LB, N), "send inaudible");
```

**With the send open the delay is audible.** Ear-confirmed by the owner at send 64.

## The report was two sessions conflated

This is the part worth keeping, because it was believed to be a live defect for **three rounds**:

```
dell11   142 x 0a05 (F.Back), 141 x 0a04 (Pan)   <- but send = 0: nothing to hear
dell13   send = 64, delay audible                 <- but NO 0a03/04/05 at all
```

The session that moved the FX knobs had no signal entering the line, so no knob could have made an audible difference. The session where the delay was audible never touched the FX knobs. *"Works in the Mixer, doesn't in FX"* is one sentence assembled from two sessions that never overlapped, and **nothing on screen or in the log showed that**.

The fix for the *understanding* was reading two lane logs side by side. The fix for the *recurrence* is `ri_core_delay_diag()`.

## The diagnostic

Portable, host-tested, emitted by the shell whenever an FX canvas changes:

```
send=64 fb=120 steps=3 pan=64 live=1 line=1
```

Every quantity that decides audibility, together, so the next "the delay does nothing" is one `grep DLY` instead of two ear-proof rounds. It **refuses rather than truncates** a cap too small to hold the line: a half-written diagnostic that looks complete is worse than none.

`t226` also walks the panel→engine path hop by hop and names the dead hop rather than reporting a symptom. All five are alive, including **hop 4, the panel bridge, which had no coverage at all** (`registry id → ri_panel_ctl_send → drain → apply` → engine struct moves → render differs for `fb` 0 vs 0.75). A green diagnostic here changes where to look: it clears the portable half and points at the shell.

## The owner's ruling, and what it cost

> "What's the most realistic option, comparable to actual hardware?"

**Web search was unavailable** this session — it returns nothing even for `test` — and the manual is not in the repository, so this could not be checked against the primary source and is recorded as resting on the local evidence above plus general synth architecture.

Ruling taken: **(c) do not auto-raise the send.** A send is a physical knob on real hardware and a send at zero gives silence, so auto-raising would be a *deviation* from the hardware rather than a match to it. The owner's P-FX3 ruling ("if the user turns on an FX panel that conflicts with another, that other one should be turned off and the newly activated FX panel should become active") was about *conflicting panels* — the inserts, which are on the signal path by definition. Delay is not, which is exactly why PCF and Comp were broken and Delay was correct. Level **64** is recorded as a documented suggestion *for the user*, not as code.

**This cost two ear-proof cycles**, because the trap was invisible rather than avoided. That is the argument for the diagnostic above, and it is also the argument against my own first answer.

## Smaller gap found on the way

`sectfx.h` names only `ONOFF` / `METER` / `DLY_STEPS` / `DLY_TRIPLET` / `COMP_GR`. The Delay panel's **Pan (index 4)** and **F.Back (index 5)** have no `RI_SFX_*` constant. `t226` names them locally rather than inventing registry knowledge; the gap is real and still open.

## Status

Delay audibility ear-confirmed by the owner. The FX knobs **in the same session as an open send** is the one combination never yet witnessed, and is the remaining check for this topic — `DLY` lines exist so it can be answered from the log rather than inferred.
