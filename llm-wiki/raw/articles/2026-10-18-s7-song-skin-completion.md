# S7 completed: a song chooses the skins, and a uniform song must not unchoose them

- Source: Dell E6320 lane e6320, ReIncarnation `0d48f32`, 2026-10-18
- Collected: 2026-10-18
- Published: 2026-10-18
- Related: `2026-09-29-s7abc-s6-research.md`, `2026-10-18-s7-skin-cycle-closed.md`
- Evidence: `docs/evidence/gui/hw-look/dell-s7-song-skin-{applied,policy}.png`

## The gap, and the trap in the obvious fix

The SKAS chunk had a codec, a writer, a reader and tests — and **no call site**. Per-section skins were a session-only UI choice. The obvious wiring (`from_song` on every load) is wrong, and wrong in a way the existing tests did not object to:

- `ri_skinassign_from_song` resolves "song carries no SKAS" to **all Classic**. That is right for a cold table and wrong for a live session: calling it on every load silently throws away the user's Ctrl+M choice each time an ordinary song plays. The symptom would be "my skins reset when a song loads", blamed on the skin system.
- So the load path got its own door, `ri_skinassign_apply_song`: `nskin == 0` → table untouched, nothing reported changed.

## One flag, two questions

`from_song`'s `dirty` means "the song names a mod that is not installed" (the registry must re-sync to fall back per part). The app needs a different fact: "did the effective table move". Comparing the tables (via `ri_skinassign_get`, so the SYNTH2 follow rule is included) gives `changed`. Conflating the two would either re-sync the registry on every load or skip the resync when a song names an uninstalled mod. The call site re-syncs on `changed`, which is the union that matters.

## Tests and mutants

- RED first: the unguarded delegate fails exactly `uniform song keeps the user's choice`, `still not classic`, `skas reports the change`.
- Mutant 1 — invert the `nskin` guard: dies on those same three.
- Mutant 2 — snapshot the table *after* the call instead of before (so `changed` is always 0): dies on `skas reports the change` and `unknown reports dirty`.

## Hardware proof, and the two lane facts it cost

A skinned song renders the 808 in `808-RI`; a two-entry playlist then cycles skinned → uniform → skinned and the log shows the policy exactly: `SKIN song=tiny-plain.rbng none` with **no** `sync=` line after it, and `changed=0` when the skinned song reloads. The capture after several uniform loads still shows the skinned 808.

Two facts that were not obvious and cost real time:

- **The playlist auto-advance is the only mid-session song load reachable without a file requester**, and it needs `song_bars > 0`. A song whose track is zeroed reports `bars=0`, and the end-of-song poller returns early on zero bars — so a "short" fixture must *truncate* the track (keep bars 0–1), not clear it. Ten minutes went into a fixture that could never advance.
- **`Break <name>` does not resolve a path** on the guest shell; `Break <number>` does. `Break SYS:Classes/ReIncarnation/RIAPP` returns `bad number` and leaves the process running, which then collides with the next launch (see the single-instance finding).

## Repo-shape note

A guest autostart script in `scripts/` fails the audit's shared-script allowlist. The gate is right: `scripts/` holds shared build and test tools. Guest deploy artifacts belong with their evidence (`docs/evidence/gui/riapp-wbstartup.script`), and moving it is cheaper than widening an allowlist for one file.