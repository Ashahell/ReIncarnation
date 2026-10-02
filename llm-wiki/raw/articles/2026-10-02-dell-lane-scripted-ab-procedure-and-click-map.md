# Driving the Dell lane by script: the verified click map, the startup clock, and the two ways a run silently produces nothing (2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane; assembling the operational knowledge scattered across the A,B,B,A and bounded-build records)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md) (the run that produced this), [2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md) (log destination and the stale-`--get` trap), [2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md) (the lane, and `T:` being RAM-backed)
- Harness: `/home/miller/Work/vms/ri-p9/ab_run.sh`, deliberately outside the repo with the other lane tooling

Everything here was established by doing it wrong first and then confirming each fact against the guest's own event log. That is the point of the record: the coordinates are *derived and verified*, not guessed, and a click that lands on the wrong control is a silent corruption of the song.

## The click map

Every coordinate below is confirmed by the event line it produces. All five tab coordinates were individually verified; the first one was verified twice by accident, which is how the coordinate space was pinned.

| target | screen | produces | verified by |
|---|---|---|---|
| Play | **(513, 80)** | `ev N … TR PLAY` | yes |
| Stop | **(560, 80)** | `ev N … TR STOP` | yes (arm A's log) |
| SYNTH tab | **(56, 166)** | `TAB page=0` | yes |
| DRUMS tab | **(122, 166)** | `TAB page=1` | yes |
| LEVI tab | **(180, 166)** | `TAB page=2` | yes |
| MIX tab | **(238, 166)** | `TAB page=3` | yes |
| FX tab | **(288, 166)** | `TAB page=4` | yes |

Click only Play, Stop and the tab row. **Never anything else**: a held delete-tap edits pattern data, so a stray click is destructive rather than merely wrong.

### How Play and Stop were found, and the wrong turn

The transport geometry is in `gui/panelgeo.c`. Play is transport index 4, `{ TR(4), RI_GEO_RECT, 0, 450, 137, 120, 76 }`, and the section geometry declares the transport canvas as `1684 × 208`. The trap is that the transport is *not* drawn at zoom 0:

```c
/* gui/zoomfit.c */
trw = sec_w(RI_SEC_TRANSPORT, RI_GEO_ZOOM_COMPACT) + 2 * RI_ART_SEAM_W;
```

and `RI_GEO_ZOOM_COMPACT` is 3. With `ri_geo_px(q, z) = q * 2 * zoom_num(z) / 16` and `zoom_num(3) = 3`, that is **`q * 3 / 8`**, so the transport draws at 632 × 78 px and Play sits at canvas-local `(169, 51, 45, 29)`.

The first attempt placed the transport at the panel root's left edge (`ROOT_INNER` = 12) and predicted Play at (203, 98). Three probes at x ≥ 390 missed, because **the transport panel is centred: it occupies screen x ≈ 347–947, not x ≈ 12**. Reading it off a magnified capture — `--ui-capture …,2` then crop and upscale with Pillow — settled it in one step. The lesson is not "compute harder", it is that a screen-space derivation needs one anchor point from the screen.

## The two ways a run silently produces nothing

**1. `Run RIPA` without the volume prefix starts nothing.** `--exec "Run RIPA"` returns `rc=0`, the process never appears, and no log is written — so the whole run looks like a flaky guest. The correct form is `--exec "Run RAM:RIPA"`. This cost two full attempts before it was spotted.

**2. The window takes ~11–15 s to open, and 6 guest `wait`s is not enough.** Each `wait` returns in ~1.06–1.14 s. A launch followed by five waits had no window, every click went nowhere, and `--ui-close` refused with *no window matching*; ten waits was enough. The harness now settles for 20 waits and then asserts the window exists before clicking.

`--ui-close` refusing is ambiguous between "the app is gone" and "the app has not started yet", and those need opposite responses. Check `--ui-windows`, not the close result.

**3. A disconnected guest agent looks exactly like a silent submit.** Added after the 2026-10-02 section work: the Dell agent went down (a guest reboot; the other session that day recorded both a bare-path launch wedging the lane and an `--ui-capture` hanging for 40 minutes), and a whole scripted run then produced *no output at all* while appearing to complete. `submit` cannot tell you the difference — a queued job and a malformed one both simply do not come back. Only `status` can:

```
python3 scripts/spike_server.py status --spool /tmp/spike_spool_laptop \
    --pairs ~/.config/spike/pairs.e6320.json
  e6320   /tmp/spike_spool_laptop   disconnected pending=6   last=0
```

So every harness here now runs a **preflight** that reads `status` and aborts with the re-dial command if the identity is not connected:

```
SYS:ATCPBIN agent <guest-ip> 9292 e6320
```

The guest was last seen at **192.168.1.60** (older notes say `.81`). Re-dialling is a manual owner action, not something a harness can do for itself.

**The general rule, and it is the one worth keeping: never pipe a submit through a filter.** Every `sub ... | grep -E "..." | tail -2` in the first version of the stage harness meant that a total failure printed nothing and looked like a run with nothing interesting to say. For a measurement harness, silence is indistinguishable from "the event did not happen" — which is precisely the thing these logs exist to detect. Each step now asserts the evidence it needs arrived (the window is listed, clicks were injected, the pulled log is non-empty, the close line is present) and the script exits non-zero instead of continuing.

## Never two instances

A stale instance surviving a close, plus a second launch, gives two RIAPPs contending for the sound card — and the second launch **truncates the ev-log** (`MODE_NEWFILE` per run), so the first run's evidence is destroyed without a word.

- Close **by index**: `--ui-close "#0"`. The title form refuses on two same-titled windows (`REFUSED: ambiguous title candidates=['RIAPP live panel']`).
- Never close index 2 on this guest: that is the agent's own console (blank title, 1366×750, `[no-close]`), and the lane depends on it.
- The harness closes `#0` twice as a pre-flight before every launch.

## The pull, and the log destination

Pull only after the writer has exited — `--get` serves a cached copy for an open path without saying so, while `Vk4aros:RIAPP-EV.LOG` fails loudly. So: close, then pull, and treat a successful `--get` on a *running* app as unverified.

Arm A (`4167e32`) logs to **`RAM:RIAPP.LOG`**, because it predates the sticky-log change; arm B and later log to **`Vk4aros:RIAPP.LOG`** and `Vk4aros:RIAPP-EV.LOG`. The harness pulls all three paths and treats a refusal as expected rather than as a failure.

`delete` on a missing file returns nonzero, so the job line reads `RESULT: FAIL` even though every action executed. That FAIL is cosmetic here — read the per-action `rc`, not the job verdict.

## The guest stalls on its own

Intermittent multi-hundred-millisecond stalls occur independently of any build, and they are large enough to invent a regression:

| run | tab-switch times (µs) | note |
|---|---|---|
| B1 | 51483, 46041, 81061, 52791, 47680 | clean |
| B2 | 51412, 46113, 91455, 52770, 52293 | clean |
| C1 | 48424, 45957, 94329, 52752, 50692 | clean |
| **D1** | 48518, 41648, **512528**, 367884, 382290 | `xruns+7/+4/+5` on the last three |
| D2 | 51407, 41688, 94273, 52957, 47772 | clean — **D1 did not reproduce** |
| D4 | 48486, **362035**, 80164, 57540, 47631 | `xruns+5` on the second |

A 5.5× "regression" with tab-attributed dropouts that never was one. **Every alarming measurement on this guest gets re-run before it is believed**, and a run that produces an outlier is excluded by name in the record rather than quietly dropped.

## Protocol, as run

Same song (the built-in default — there is no `ev N SONG load` line), both arms given the identical script:

1. `delete RAM:RIAPP.LOG`, `delete Vk4aros:RIAPP.LOG`, `delete Vk4aros:RIAPP-EV.LOG`
2. pre-flight `--ui-close "#0"` twice
3. `Run RAM:<arm>`, settle 20 waits, assert the window
4. click Play, then the five tabs with 4 waits (~4 s requested) between each
5. click Stop, settle, `--ui-close "#0"`
6. pull all three log paths

A,B,B,A alternating, two runs per arm. Deviation from the advisor's 4 s gap: the guest's `wait` returns faster than it reports, so switches actually landed ~0.7 s apart. Both arms got the identical script so the comparison holds, but the absolute numbers do not transfer to a human-paced session.

## Method findings

- **Derive injection coordinates from the geometry, then confirm them against the event log.** A click verified by the event it produces is not a blind click; a click derived only from arithmetic missed three times because the panel was centred.
- **`rc=0` from `Run` means the command was accepted, not that a process exists.** A launch must be confirmed by `status` or `--ui-windows`, never by the return code.
- **Refuse to believe a single dramatic measurement on shared hardware.** It cost one wasted investigation and it is the only reason D1 is documented instead of quietly deleted.
- **Index-based addressing beats name-based addressing when names can collide**, and one of the things you can close by index is the channel the whole lane runs over. Check what is at each index before automating a close.
- **A measurement harness must fail loudly, and a filter in the pipeline is how it fails silently.** Piping `submit` into `grep` to keep the log readable also discards the evidence that the command failed. A script that cannot distinguish "no result" from "no result because the lane is dead" will quietly produce a series of empty runs that look like measurements.
- **Ask the server, not the client, whether the other end is alive.** `submit` reports its own timeout and nothing about the guest; `status` reports per-identity connection state and a pending count. One line of preflight would have turned a ten-minute mystery into an immediate, actionable error.

## Standing gaps

- The ~0.7 s effective tab gap versus the requested 4 s is not understood; `wait` appears not to sleep for its full duration.
- The stall's cause is uninvestigated and it confounds every measurement taken on this guest.
- Only Play, Stop and the tab row are automated. Anything else would need the same derive-then-verify treatment, and some controls are destructive.
- A song is never loaded, so Zombie Nation comparisons against the older records remain unbridged.
- **The section-level render split is built, tested and mutation-proven, but NOT yet measured on target** — the lane went down before the guest run. There is no device number for "which of the five voice engines carries the cost". Host laws are complete; the measurement is blocked, not skipped.

## See Also

- [Scripted A,B,B,A: the governor arm is a real win, the repaint policy is a real regression](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md)
- [The Dell lane has two silent traps: the audit's link gate builds ABIv1, and `--get` serves stale bytes for a file the guest still has open](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md)