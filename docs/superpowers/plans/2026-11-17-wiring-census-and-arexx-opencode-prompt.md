# Prompt for OpenCode: every control wired correctly and completely, and an ARexx port for RIAPP

> Written 2026-11-17 by the Claude advisor session. The owner asked for:
> *"make sure everything is wired up correctly and completely. Also,
> reincarnation needs an AREXX port."*
>
> **Two parts.** Part W (W0–W4) is the wiring census and its fixes. Part X
> (X0–X4) is the ARexx port, and it ends by joining the census as one more
> door. Each phase ends with a commit and a short report. If you run short of
> context, stop at a phase boundary and write a handoff.
> **Do not push.** The owner reviews before anything is pushed.
>
> **Same rules** as every prompt: check each fact in §0 yourself before you
> rely on it; a result that contradicts this prompt stops you and you say
> which statement it contradicts; nothing outside the repo is load-bearing.

---

## 0. Facts (read 2026-11-17 at `c9bd66f`; check each before relying on it)

### The registry

- `gui/ctlreg.h` / `gui/ctlreg.c` is the single control table
  (`struct RICtlDef`, `ri_ctlreg_count()`, `ri_ctlreg_at()`). Measured at
  `c9bd66f`: **510 rows**, **102** with a MIDI CC, **321** automatable.
  - By bind: `NONE` 215, `303` 14, `808V` 21, `808ALL` 1, `909V` 22,
    `909HAT` 1, `FX` 18, `PAN` 5, `SEND` 5, `INSERT` 16, `TEMPO` 1,
    `LEVEL` 6, `LEVI` 180, `MUTE` 5.
  - By section (`RI_SEC_*`, 21 sections): `LEVI` 252, `909` 46, `808` 44,
    `SYNTH1`/`SYNTH2` 30 each, `TRANSPORT` 16, the rest 4–8.
  - Header rule (ctlreg.h:6): *"A control the engine cannot honour yet is
    RI_BIND_NONE: the GUI shows it disabled, it never becomes a silent
    no-op."* 215 `NONE` rows is the first thing to explain: some are
    press/state-path controls (steps, transport, pattern), some are
    panel-local actions, and some may be controls that look live and are not.
- A row is sent through `ri_panel_ctl_send()` / `ri_panel_ctl_send_key()`
  (`gui/panelctl.h:18,21`).

### What already checks wiring (all in `scripts/ri_audit.sh`)

Baseline: `bash scripts/ri_audit.sh` at `c9bd66f` (with the new `LICENSE`,
`NOTICE` and this prompt in the working tree) ends `AUDIT 0/0 PASS`, rc 0,
about 3 min. Re-run it first; if it is not green, stop.

- `t116_panel_wiring` — for every **value** control (KNOB/FADER/SWITCH/
  SELECTOR/DISPLAY) with a lane key, one bridge send equals the direct setter
  (whole-engine memcmp). Step/button/pattern/transport controls are **out of
  its scope by design**.
- `t224_decorative_sweep` — controls that look live and are not. Its header
  lists **four** such controls found in one session (FX On/Off switches,
  displaced FX lamp, mixer insert rockers, strip mute). That count is the
  reason this census exists.
- `t170_ctlreg_index`, `t60_ctlreg`, `t61_panelgeo`, `t73_midimap`,
  `t77_autolane`, `t231_automation_readback`, `t232_midi_push`,
  `t221_keybed`, `t239_keybed_drawn_is_playable`, and the Levi `t1xx` set.
- **Not gated:** `docs/evidence/levi-wiring/2026-10-08-advisor-audit/
  levi_audit.c` (Levi only). Its last output: *rows: 180 keyed, 19
  default-mismatch, 2 state-dead, 79 silent*; *encoders: 893 live slots,
  201 default-mismatch, 96 nomove, 10 nokey, 10 state-dead, 559 silent*.
  STATE-dead and DEFAULT-mismatch are bugs by its own definition; SILENT is
  not (it needs an ear).
- The to-do list (`docs/2026-09-24-improvement-todo.md`) still has open:
  BPM-flagged automation keys with no registry row (~321), the Mixer/Master
  COMP tab On/Off as an unwired lamp (~319), the seven MIDI settings with no
  panel row and no runtime path (~313), the stale-bounds lesson (~283: *"a
  comment in the tree claims a guard that does not exist"*).

### The doors a control can be reached through

Each is a separate path into the engine or out of it. A control is wired
**completely** only when every door that applies to its row reaches the same
engine state and every feedback path shows it.

| # | Door | Where |
|---|------|-------|
| D1 | Panel press / drag | `gui/panelui.c`, `gui/panels.c`, `ri_panel_ctl_send` |
| D2 | Levi page encoders | `gui/sectlevi.c` (`ri_slevi_*` page model) |
| D3 | MIDI CC in | `gui/midimap.c` (`ri_midi_msg`), `midi_io/` |
| D4 | MIDI out (CC/notes/MMC) | `midi_io/midi_out.c`, `midi_devout.c`, `midi_mmc.c` |
| D5 | Song automation record + playback | `engine/seq/autolane.c`, `ri_ctlreg_auto_id` |
| D6 | Save / load (RBNG) | `project/rbng.c` |
| D7 | Startup default sync | `app/riapp.c` ~4198–4257 (the startup burst, Levi adopt) |
| D8 | Feedback: lamp, LED, meter, repaint | `gui/livestate.c`, `t227`/`t228` |
| D9 | Computer keyboard | `gui/keymap.c` |
| D10 | Menus | `app/riapp.c` (MUI menu strip) |
| D11 | Undo | `project/undo.c` |
| D12 | ARexx (Part X adds it) | `project/arexx*.c` |

### The lane

- **The Dell now boots the ABIv1 SMP stick** (since 2026-11-17; Kickstart
  51.51, `e1000.device` 1.4, agent `e6320` on `192.168.1.81:9292`). Build Dell
  binaries with **`scripts/ri_build_aros.sh riapp`** (v1), **not**
  `ri_build_v11.sh`. RIAPP built this way at `4a12fe5` (1,243,080 B) ran with
  its window up and sound, by the owner's check.
- Push with `lane/ri_dell_push.sh <file> RIAPP` (`Run RAM:RIAPP`; never
  `Run <absolute path>`). Get with `lane/ri_dell_get.sh`.
- `SYS:Classes/ReIncarnation/` (Songs/Mods/Packs) is **not** on this stick;
  RIAPP falls back to the built-in demo.
- **Never `avail flush` on the Dell.** A new binary is a new `RAM:` push, or
  a reboot by the owner.
- Ear tests are the owner's. You prepare them; you do not report an ear
  result you did not get from the owner.

### ARexx — what exists

- `project/arexx.c` — exact-string parser. Commands: `OPENSONG <path>`,
  `PLAY`, `STOP`, `EXPORTWAV <song> <wav>`, `SETRPPARAM <ctl> <val>` (ctl
  0..65535, val 0..127). Optional leading port token `REINCARNATION` /
  `REINCARNATIONAROS` (deprecated alias).
- `project/arexx_dispatch.c` — pure: parsed command → reply **text** only
  (`OK PLAYING`, `OK STOPPED`, `OK OPEN …`, `OK EXPORT …`, `OK PARAM …`;
  rc 0/10/12). **It has no effect.** Nothing plays, stops or opens.
- `project/arexx_aros.c` — `ri_arexx_handle(line, reply, sz)`, AROS-only
  (`#error` guard). Its header says the MsgPort, the wait loop and
  `ReplyMsg` were **DEFERRED** (`docs/evidence/formats/beta-exit.md` row
  34: *"create REINCARNATION port, drive all 5 commands from Rexx, assert
  reply texts equal the pinned dispatch strings"*).
- Host-tested by `tests/unit/t1_rel.c`; AROS compile-only for `arexx.c` in
  `ri_audit.sh` Phase 13 (~1544). **None of the three files is in RIAPP's
  link list** (`scripts/ri_build_aros.sh` ~155).
- Spec (`docs/superpowers/specs/2026-09-20-reincarnation-spec.md` l.69):
  primary port **`ADDRESS REINCARNATION`**. The manual
  (`docs/ReIncarnation.guide` node `ARexx`, l.85) documents the five commands.
- RIAPP's MUI app has `MUIA_Application_Base "RIAPP"` and no `SingleTask`
  (`app/riapp.c:4437`). Zune therefore names the app `RIAPP.1`, and with
  `MUIA_Application_UseRexx`/`Commands` its Rexx port would be **`RIAPP.1`,
  not `REINCARNATION`** (`workbench/libs/muimaster/classes/application.c`
  1286–1297, 1582–1597 in the v1 AROS tree).
- Zune's own `MUIA_Application_Commands` table **prefix-matches** command
  words (its autodoc, *BUGS/NOTES*: *"prefixes can match an earlier longer
  table name"*) and does not reliably check ReadArgs failure. Our parser is
  exact-string. Do not route our commands through the Zune table.
- RIAPP's main loop builds its own wait mask: `sigs |= SIGBREAKF_CTRL_C |
  timer bit; sigs = Wait(sigs);` (`app/riapp.c` ~5034) and hands the rest to
  `MUIM_Application_NewInput` (~4623). A port signal fits there.
- **On the Dell stick:** `SYS:Rexxc/RX` and `RXLIB` exist and
  `rexxsyslib.library` 44.4 is in `SYS:Libs`, but **there is no Rexx
  interpreter**: `rx "say 6*7"` → *"Could not start RexxMast; no Rexx
  interpreter seems to be installed"*. Regina lives in AROS **contrib**
  (`aros-development-team/contrib`, `regina/`, LGPL), not in the v1 tree.
  So a script-level proof needs Regina on the stick, which is the owner's
  call (X4). The port itself can be proven without it (X3).
- Portability rule: Amiga includes only in `platform/aros/`,
  `project/datatypes/`, `project/arexx_aros.c` and the legacy proof apps
  (`docs/superpowers/plans/2026-09-26-portability-plan.md` l.37, 63). No
  `"SYS:`/`"RAM:`/`"ENV:` literals outside `platform/aros/`
  (`ri_audit.sh` ~969). `scripts/` holds exactly five files (gated).
- Test numbers in `tests/unit/` run to **242**. Grep for an unused number
  before adding one.

### Licence

The repo is now **AROS Public License 1.1**: `LICENSE` (verbatim from the v1
AROS tree's `LICENSE`, sha256 `825705f4…`) and `NOTICE` (Exhibit A; the
`reference/packs/` layers stay CC0, as each layer's `.txt` says). Do not edit
either. New files need no per-file header unless the owner asks.

---

## Part W — the wiring census

### W0 — the census tool (host only)

Write a host test, `tNNN_wiring_census`, that walks **every** registry row
(all 21 sections, not only Levi) and, for each door D1–D11 that applies to
the row, drives the control to an alternate value through that door and
records one verdict:

- `OK` — engine state moved, and equals the D1 result (whole-engine memcmp
  on twin fresh engines, the t116 method);
- `DEAD` — the door accepts the input and the engine state does not move;
- `DIVERGE` — the state moves, but not to the D1 state;
- `NOFEEDBACK` — the state moves and the lamp/LED/meter/repaint does not;
- `DEFAULT` — the panel default sent to a fresh engine moves the engine
  (panel shows a value the engine is not at);
- `NOROUNDTRIP` — save → load (D6) or record → play (D5) does not restore it;
- `NA` — the door does not apply, with the reason taken from the row
  (`automatable=0`, `midi_cc=NONE`, kind), never from a hand list;
- `BINDNONE` — `RI_BIND_NONE`, with a sub-class: `PRESSPATH`
  (step/transport/pattern), `PANELLOCAL`, or `UNEXPLAINED`.

Output a table `docs/evidence/wiring/2026-11-NN-census/census.tsv` (one row
per control, one column per door) plus a summary count per section and per
verdict. The test **passes** today if it runs and writes the table; it does
not yet fail on verdicts (that is W3).

Reuse `levi_audit.c`'s method for D2 and SILENT, but SILENT stays a
**report**, not a failure: a stored, delivered, inaudible control can be
correct by design.

**Controls (must be in the commit):**
- a positive control per verdict class: at least one row that the tool
  reports `OK`, and a deliberately broken copy of one known-good row (unbind
  it in a test-only table) that the tool reports `DEAD` — a census that
  never says `DEAD` proves nothing;
- `t224`'s four historical defects, re-introduced one at a time in a scratch
  build, each caught.

**Gate W0:** the test builds and runs under `ri_build_host.sh`, both
controls fire, `ri_audit.sh` stays green, and the census table is committed.

### W1 — classify (host only, no fixes)

For every row that is not `OK` or `NA`, write one line in
`docs/evidence/wiring/…/classify.md`: verdict, door, the code path that
loses it (file:line), and **bug / by-design / needs-owner**. `BINDNONE
UNEXPLAINED` rows are bugs until shown otherwise. Re-check the to-do items
~313, ~319, ~321 against the table and say which rows they are.

**Stop and ask the owner** before W2 with the bug count per section and the
needs-owner list.

### W2 — fix, one section per commit

Fix the bugs W1 found, **one section (or one door) per commit**, each with
the census delta in the message (`before → after` counts). Rules:

- A control that cannot be honoured yet becomes `RI_BIND_NONE` **and**
  disabled in the GUI (the header rule). Never leave it looking live.
- Widening a range to make a SILENT control audible needs an E1 reason (a
  manual page), not a census row.
- No new registry IDs without the owner (stable IDs, spec l.265).
- `ri_audit.sh` green after every commit; draw-hash pins that move must be
  explained in the commit, not just re-pinned.

### W3 — make it a gate

Turn the census into a failing gate: `DEAD`, `DIVERGE`, `NOFEEDBACK`,
`DEFAULT`, `NOROUNDTRIP` and `BINDNONE UNEXPLAINED` fail unless the row is in
a committed allow-list with a reason and a date. The allow-list starts as
the W1 by-design and needs-owner rows, nothing else. Add it to
`ri_audit.sh`.

**Gate W3:** the gate fails on the W0 broken-row control and passes on HEAD.

### W4 — Dell proof and the owner's ear list

Build RIAPP v1, ABI-check it (zero `mov %rax,%r12`; prove the grep finds a
known positive first), push it to the Dell, and give the owner:

- a short ear list of the controls W2 changed, grouped by section, each with
  what to do and what to listen for (yes / no / subtle), in the style of
  `docs/evidence/midi/levi-audibility/README.md`;
- the SILENT set per module, so the owner can say which *should* sound
  (to-do ~320).

You report the owner's answers; you do not supply them.

---

## Part X — the ARexx port

### X0 — design note (host only)

One page, `docs/superpowers/specs/2026-11-NN-arexx-port.md`, settling:

1. **Port name.** Spec says `REINCARNATION`. Recommended: RIAPP creates its
   own public `MsgPort` named `REINCARNATION` (`Forbid(); FindPort();
   AddPort(); Permit();`), refusing to start a second port if the name is
   taken, and keeps `REINCARNATIONAROS` only as the parser alias. Not Zune's
   `UseRexx` (it would be `RIAPP.1`, and the Base also names MUI prefs).
   Leave `MUIA_Application_Base` alone.
2. **Command set v1.** The five existing commands, plus what the census says
   is missing for scripting to be useful. Recommended additions:
   `SETCTL <reg_id> <value>` (any registry row through
   `ri_panel_ctl_send`, range-checked against the row, so ARexx is one more
   door, not a back door), `GETCTL <reg_id>` (RESULT = value), `TEMPO <bpm>`,
   `STATUS` (RESULT = `PLAYING|STOPPED <bpm> <song>`), `QUIT`.
   `SETRPPARAM`'s 0..127 contract stays for compatibility; say how it maps.
3. **Threading.** Commands run on the **main task** in the existing
   `Wait()` loop, never on the audio path. Every effect goes through the
   same functions the GUI uses (`ri_core_play`, `ri_core_stop`,
   `ri_core_load_song`, `ri_live_set_bpm`, `ri_panel_ctl_send`), so the
   panel, lamps and automation see it.
4. **Replies.** `rm_Result1` = rc (0/10/12, plus 20 for "failed to act");
   `rm_Result2` = `CreateArgstring(text)` **only** when `RXFF_RESULT` is set.
   Every message is replied, including malformed ones. Messages still queued
   at exit are replied with rc 20 before `RemPort`.
5. **EXPORTWAV** — offline render through the existing render path, or
   deferred. Say which, with the reason.

**Stop and ask the owner** to approve the note before X1.

### X1 — dispatch with effects (host)

Split `arexx_dispatch` into the pure reply contract (keep `t1_rel`'s pinned
strings byte-exact) and an effect interface (function pointers the app
fills in), so the host test can assert that `PLAY` calls play, `SETCTL`
calls `ri_panel_ctl_send` with the right `reg_id`/value, out-of-range values
are refused before any call, and an unknown command calls nothing. New host
test `tNNN_arexx_effects`.

### X2 — the port in RIAPP (AROS)

Finish `project/arexx_aros.c`: open `rexxsyslib.library`, create and
publish the port, add its signal to RIAPP's wait mask (`app/riapp.c`
~5034), drain with `GetMsg`, `IsRexxMsg` check, `ARG0` → `ri_arexx_handle`
→ effects → `ReplyMsg`. Add the three ARexx TUs to RIAPP's link list in
`scripts/ri_build_aros.sh`, keep them out of the host build (audit gate
~1532 pattern), keep the `#error` guard.

**Gate X2:** RIAPP builds v1 with 0 unresolved and 0 r12 moves;
`ri_audit.sh` green.

### X3 — prove the port without an interpreter (Dell or riqemu1)

There is no Rexx interpreter on the stick (§0). Write a tiny AROS sender in
`lane/rxsend/` (the `lane/blitprobe` precedent: a probe, not shipped):
`CreateRexxMsg` + `CreateArgstring`, `RXCOMM | RXFF_RESULT`, `FindPort
("REINCARNATION")` under `Forbid`, `PutMsg`, wait for the reply, print rc
and RESULT, `DeleteArgstring`/`DeleteRexxMsg`. ABI-check it like RIAPP.

Prove on the lane, with RIAPP running:
- every v1 command, and its reply text equal to the host-pinned string;
- `PLAY`/`STOP` change the transport **and** its lamp (ask the owner to
  look, or use the ev-log);
- `SETCTL` on one row per section lands (census row turns `OK` for D12);
- a malformed line and an unknown command get rc 10, RIAPP keeps running;
- RIAPP quit with a message queued: the sender gets rc 20, no hang;
- a second RIAPP refuses the port name cleanly.

Then add D12 to the census (W0's table) and re-run W3's gate.

### X4 — Regina (owner's call)

Regina is in AROS contrib, not on the stick. **Ask the owner** whether to
build it for ABIv1 and stage it. If yes: build it out of tree, ABI-check
it, stage only what `RX` needs (`RexxMast` and the library it starts),
reboot (owner), and prove `rx "ADDRESS REINCARNATION; 'STATUS'; say
result"` plus one script per command. If no: record that the port is proven
by `lane/rxsend` only, and say so in the manual's `ARexx` node.

Update `docs/ReIncarnation.guide` (`ARexx` node) with the final command
set, the port name, the reply codes, and one example script.

---

## 1. Stop and ask the owner when

- W1 is done (bug count, needs-owner list) — before any fix;
- a fix needs a new registry ID, a range change, or changes what a control
  *does* rather than whether it arrives;
- X0's design note is written — before X1;
- anything needs the Dell rebooted, the stick written, or Regina staged;
- a result contradicts this prompt. Say which statement it contradicts.

## 2. Report, per phase

Commit hash, what changed, gate output (the lines, not "green"), the
census counts before → after, binary sizes and sha256 for anything staged,
and what is **not** proven yet.
