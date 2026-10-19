# Prompt for OpenCode: RIAPP_MIDI_OUT, and RIAPP's own MIDI out proved on the Dell

> Written 2026-10-19 by the Claude advisor session. The owner asked for:
> *"Do the RIAPP_MIDI_OUT setting and prove RIAPP's own note/CC and MMC out on
> the Dell while the rtpmidid setup is live."*
>
> Background: `docs/superpowers/specs/2026-10-19-r12-network-midi-assessment.md`
> and `docs/evidence/midi/r12/`. Read both first.
>
> **Phases O0–O4.** Each phase ends with a commit and a short report. If you
> run short of context, stop at a phase boundary and write a handoff.
> **Do not push.** The owner reviews before anything is pushed.

---

## 0. What the code does today (read 2026-10-19 at `77601f9`)

Check each of these yourself before relying on it.

1. **One cluster for everything.** `RIAPP_MIDI_IN` (default `riapp`, in
   `s_mset.cluster`) feeds:
   - the receiver: `ri_pal_midi_open_in(s_mset.cluster, …)`, `app/riapp.c`
     ~3266;
   - the sender task start: `ri_pal_midi_send_start(s_mset.cluster, …)`,
     ~3286;
   - MMC out: `ri_pal_midi_send(s_mset.cluster, …)` from the main task, ~772.
2. **The sender ignores its port.** `ri_pal_midi_send_start()` does
   `(void)port;` (`platform/aros/midi_camd.c` ~598). Its `send_sink()` calls
   `ri_pal_midi_send(0, …)`, and a NULL port means `s_cluster`: the **input**
   cluster that `open_in` stored. With `RIAPP_MIDI_IN=applemidi.in.0`, clock
   and device notes would therefore go to the **in** port. That is why N3
   pointed `RIAPP_MIDI_IN` at `applemidi.out.0`.
3. **One TX link, refused on a mismatch.** `ri_pal_midi_send()` builds a
   single sender link (`s_tx`, destination in `s_txcluster`) on first use and
   **refuses** any later send to a different cluster. That refusal is the
   2026-10-12 crash fix; read the comment at ~322 before touching it. Never
   swap or free the link in a send.
4. **Device notes and clock share one ring** (`s_mout`) and one sender task.
   `app/core/live_driver.c` `livedrv_devout()` writes 303/808/909 notes into
   it, so notes inherit the same 5.33 ms block quantisation as clock (N3).
5. **MMC out has never worked, for two independent reasons.** It is recorded
   as landed on 2026-10-10.
   - **The frame is wrong.** `emit6()` (`midi_io/midi_mmc.c` ~148) writes
     `F0 7E 7F 06 <cmd> F7`, with **play = 0x01 and stop = 0x02**. MMC is
     **Universal Real Time** SysEx: `F0 7F <device> 06 <cmd> F7`, with
     **01 = Stop, 02 = Play**, 03 = Deferred Play. **Our own inbound parser
     agrees with the spec**: `midi_mmc_cmd()` (`midi_io/midi.c` ~32) requires
     `p[1] == 0x7F` and maps 02/03 to play and 01 to stop. So RIAPP would
     reject its own MMC out.
   - **t191 pins the wrong bytes** (`tests/unit/t191_mmc_out.c` ~36–42:
     `0x7E`, play `01`, stop `02`), so the suite is green on a wrong wire
     format.
   - **The PAL refuses it anyway.** `ri_pal_midi_send()` returns 1 for
     `len > 3` (`midi_camd.c` ~314). It packs one short message for
     `PutMidi`, and the MMC frame is 6 bytes. On AROS every MMC send is
     refused (`evlog MMC tx=… refused`). SysEx needs CAMD's SysEx call.
6. **CC out does not exist in the app.** `ri_devout_cc()`
   (`midi_io/midi_devout.c` ~135) is implemented and host-tested, but **has
   no caller** outside tests. Knob moves and automation send nothing. *Proving*
   CC out first needs it *built*, and that is a design decision for the owner
   (phase O3).
7. **Settings:**
   - `RIAPP_MIDI_CLKOUT` (clock);
   - `RIAPP_MIDI_DEVOUT` (device notes and the program change; GM program 38
     on `RIAPP_MIDI_NOTECH`);
   - `RIAPP_MIDI_NOTECH` (the melodic channel; 0 = unassigned, and then no
     melodic note leaves; drums always use channel 10);
   - `RIAPP_MIDI_MMCOUT`.

   All are read with `GetVar` at startup, so a **running RIAPP never sees a
   later `setenv`**. RIBOOT starts one at boot with defaults. Stop it
   (`Break NAME=SYS:Classes/ReIncarnation/RIAPP C`) and start a fresh one
   (lesson from N1j).

## 1. Lane rules (unchanged)

- **Never `avail flush` on the Dell.** Libraries reach `LIBS:` only through
  an owner reboot.
- Only one RIAPP at a time. Use `-i` on the instance grep. Don't drive a
  daemon through `--run-script`: use `Run >file …`.
- **Hash-verify every binary you deploy, and record build hash = `RIAPP LOG
  build=`.** N1j's first pass ran on a stray binary.
- Keep host logs under `/home` (`~/.worktrees/r12-logs/`), never `/tmp`.
- `rtpmidid` must keep running with `--rtpmidi-discover false`. Don't create
  test ALSA clients while a session is up (N1j).
- **Before blaming AROS, rule out the caller:** constants, return
  conventions, NULL locks, late `IoErr()`, `WriteChars` vs `Write(fh)`.
- Covers stay in `songs/local/`. Nothing derived from them is committed.

## 2. Phases

### O0 — the setting and the PAL port (host-testable, no lane)

1. Add **`RIAPP_MIDI_OUT`**: a second cluster in the MIDI settings,
   **defaulting to the `RIAPP_MIDI_IN` value**, so every existing setup
   behaves exactly as before. Log it in the existing `RIAPP midi in=…` line
   (`out=`).
2. **The sender must use the cluster it is given.**
   - `ri_pal_midi_send_start(port, …)` stores `port`, and `send_sink` sends
     to it.
   - MMC out passes the **out** cluster.
   - `ri_pal_midi_send(NULL, …)` keeps its current meaning for MIDISEND,
     which sends without opening.
   - Update the PAL header comment (`ri_pal_midi.h` ~33, *"port is a camd
     cluster name or NULL for the one ri_pal_midi_open_in() was given"*) so
     it states what the code now does.
3. **Keep the one-link rule.** The sender task and the main task's MMC must
   name the **same** out cluster, so the mismatch refusal never fires in
   normal use. If anything can still reach the refusal, say what. Don't
   weaken it.
4. **Tests:**
   - Host unit tests for the settings: out defaults to in, and an explicit
     out overrides it. Each needs a mutant that dies (out ignored; out
     defaulting to `riapp` instead of to in).
   - If a PAL-level behaviour can't be tested on the host, say so plainly
     rather than writing a test that asserts the mock.
5. **Fix MMC out** (see §0.5):
   - The frame becomes `F0 7F 7F 06 <cmd> F7`, with play = `02` and
     stop = `01`.
   - Rewrite t191 to assert the **spec** bytes, and add a **round trip**:
     every frame `midi_mmc_out_*` emits must be accepted by our own
     `midi_mmc_cmd()` with the matching meaning. Then the two halves can't
     drift apart again. Add mutants: `7E` restored, and play/stop swapped.
     Both must die.
   - Give the AROS PAL a SysEx path: frames longer than 3 bytes go out with
     CAMD's SysEx call, under the same `tx_claim` and the same one-link rule,
     never as a second link. Check the CAMD autodoc in the tree for the exact
     call and its argument order before writing it.
   - Correct the ledger's 2026-10-10 "MMC out landed" entry: it never reached
     a wire.
6. `scripts/ri_audit.sh` must pass **in full**. Run it, don't skip it.

**Gate O0:** audit `0/0 PASS`, the new tests and mutants listed, the default
cluster behaviour unchanged, and MMC out emitting spec bytes that our own
parser accepts.

### O1 — device notes and MMC out on the Dell

Deploy the O0 build (hash-verified). Use the **clean** shimmed `netmidid`
(122448 B, `c71fe8ff…` from N0), **not** the instrumented one, and record
its sha. That also closes the "gate ran on a debug build" gap. Re-check N1
gate check 1 (the session connects) on it first.

Start a fresh RIAPP with:

```
RIAPP_MIDI_IN=applemidi.in.0
RIAPP_MIDI_OUT=applemidi.out.0
RIAPP_MIDI_CLKOUT=1
RIAPP_MIDI_DEVOUT=1
RIAPP_MIDI_NOTECH=1
RIAPP_MIDI_MMCOUT=1
```

On the host, run `aseqdump -p <the arosbox port>` with its output to a file
under `/home`. Then:

1. **Transport Play** on a song that uses the 303 and the drums. Use a
   non-cover song from `songs/`, or a short pattern you write. Never a
   `songs/local/` cover in committed evidence.
2. Let it play about 60 s, then **Stop**.

**Gate O1, each item with the lines that show it:**
- **Program change** 38 on channel 1, **exactly once**, before the first
  melodic note.
- **303 notes** on channel 1. **808/909 notes** on channel 10.
- **Every note-on has its note-off.** Count on/off per channel and report
  anything stuck.
- **MMC Play and Stop** arrive as SysEx: `F0 7F 7F 06 02 F7` (Play) and
  `F0 7F 7F 06 01 F7` (Stop), the bytes the O0 fix emits.
  - **Unverified:** that `rtpmidid` forwards SysEx to ALSA. If no SysEx
    shows up, establish which end drops it (netmidid's receive log or
    rtpmidid's log) before calling it a RIAPP fault.
- **Clock** still flows on the same port (count and rate only; N3 already
  measured the spread).
- **Accounting:** the guest's `RIAPP clk: … sent=` and the device-out
  emitted/refused counters against what `aseqdump` saw. Explain any
  difference.
- **No echo:** during this output-only run, `NOTEIN seen=` must **not** move.
  If it does, our own output is coming back to us. Stop and report.

### O2 — both directions at once (the point of the setting)

Same instance and same settings. While it plays, `aplaymidi` a short file
from the host into the Dell, as in the N1j check 3.

**Gate O2:**
- `NOTEIN played=` moves on the inbound notes;
- the output keeps flowing, with no gap in clock or notes around the inbound
  burst;
- `RIAPP clk: err=` stays 0.

That is R5 and R6 together on separate clusters, which neither N1j nor N3
tested.

### O3 — CC out: a design note for the owner, NOT code

`ri_devout_cc()` has no caller, so there is nothing to prove until it is
wired. Wiring it is a feature with real choices, so **write the design and
stop**:
- **Sources:** which changes produce a CC. Panel moves, song automation, and
  inbound MIDI CC/G7? The registry's `cc` column already maps the 99 G7
  controls (`gui/ctlreg.c`).
- **Echo rule:** a CC that **arrived** over MIDI must not be sent back out,
  or a DAW that echoes creates a loop. MMC already has this rule
  (`s_mmc_echo`, `app/riapp.c` ~760). Say how CC gets the same.
- **Rate:** a knob drag fires many `CTL` events. Send every change, or
  coalesce per block? What does the 64-byte pump cap mean for a burst?
- **Channel:** the G7 remote channel, or per device?
- **Path:** which task writes the ring. The render is the ring's only
  producer today (SPSC). A main-task writer would break that. Propose the
  correct route.

**Gate O3:** `docs/superpowers/specs/2026-10-xx-cc-out-design.md` with each
choice laid out as options plus one recommendation, for the owner's
decision. No code.

### O4 — write-up

- Evidence goes to `docs/evidence/midi/r12/r12-o*.md`, with raw logs under
  `/home` and listed.
- Update the assessment's "app change" result note, the interop R4/R6 lines
  and the ledger. Remove the `ri_pal_midi_send_start` "ignores its port"
  trap notes, now that it's fixed, and say where it was fixed.
- **Name what is not proven:** CC out (not built), a real DAW, and clock-in
  at 140 BPM.

## 3. Stop and ask the owner when

- a Dell reboot is needed (camd, or a wedged lane);
- anything has to be installed on the host;
- O1's echo check fails, or SysEx can't be placed;
- O3 is written (the owner decides; you don't build it);
- a result contradicts this brief. Say which statement it contradicts.
