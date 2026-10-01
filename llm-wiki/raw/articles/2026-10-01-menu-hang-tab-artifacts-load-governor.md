# Menu hang + tab-switch artifacts: render load governor and damage-box fix (2026-10-01)

- Source: ReIncarnation session, 2026-09-30/10-01 (commit `fe36b9d`, pushed)
- Collected: 2026-10-01
- Published: 2026-10-01
- Related: [2026-09-30-songs-playlists-zombie-nation.md](2026-09-30-songs-playlists-zombie-nation.md), [2026-09-30-method-findings-songs-session.md](2026-09-30-method-findings-songs-session.md)

## Owner report (Dell E6320)

- Opening the right-click menu left RIAPP unresponsive; it could not be closed; the mouse still moved; no software failure.
- Switching tabs sometimes rendered only part of the screen, leaving the previous tab visible.
- The Dell then stopped answering the agent and needed an owner reboot.

## Tab-switch artifacts (root cause found)

- During play, `meter_round` queues partial repaints (`ri_rsection_refresh_box`) for the MIX meters, MASTER meters and the 808/909 chase lamps, including canvases on hidden tabs.
- `MUI_Redraw` on a hidden object is a no-op, so the damage box (`dmg_valid`) stayed set.
- At the page switch, Zune's full draw (`MADF_DRAWOBJECT`) reached `draw_frame`, which honoured the stale box and repainted only that box.
- Fix (`gui/widgets/rsection.mcc.c`): `MUIM_Draw` clears the box unless the flags carry `MADF_DRAWUPDATE`. `ri_rsection_refresh_box` ignores hidden canvases, because the Show draw covers them.
- Verified in a private VM: SYNTHS, DRUMS, LEVI, MIX and FX all repaint completely while playing.

## Menu hang (probable cause, not reproduced)

- **Not reproduced.** A private, network-isolated QEMU VM never hung. Tested: menus idle and playing, a quick click, every tab, Songs → Load Song... (ASL requester), and the View menu.
- **AROS facts checked:**
  - The menu task holds `LockLayerInfo` only around `WhichLayer`.
  - The input handler holds `MenuLock` while menus are active.
  - Zune never uses MENUVERIFY.
  - The pointer is moved by Intuition's input handler (`SetActiveMonPointerPos`) at priority 20. While menus are active, every input event goes to the menu task.
  - Hence "the pointer moves, nothing else responds" means the menu task (priority 0) gets no CPU, or is blocked.
- **Probable cause:** the AHI render task runs at priority 10 and renders one half per SoundFunc signal. If a half takes longer than the buffer period, the signal is always pending, so the task never sleeps. Every priority-0 task then starves: the menu task, the app, Wanderer and the agent.
- **Load evidence (host `songplay`, AMD Ryzen 7 9800X3D):**
  - Zombie Nation: 260.9 s of audio rendered in 28.4 s.
  - RIAPP demo: 29.4 s rendered in 3.6 s.
  - Both are roughly 11–12% of one core, i.e. about 0.6 ms per 256-frame buffer (5333 us).
  - The Dell's i5-2520M is several times slower per core.
  - Earlier Dell heartbeats, before the Levi fidelity work, showed `render_max=164 us`.
- **gprof of the demo render:**
  - `rb303_render` 42% (`rb303_filter_step` with `ri_tanh`/`ri_exp` per sample);
  - `levi_voice_render_sum` 32%;
  - `rb808_render_mix` 21% (`rb808_pitch_hz` per sample per voice).

## Load governor (fix)

- **Portable driver** (`app/core/live_driver.{c,h}`): load = render time / buffer period, per mille, smoothed 1/8.
  - At `RI_LIVEDRV_OVER_PM` 850 the driver flags overload for `RI_LIVEDRV_OVER_US` 2 s.
  - It then probes again with the smoothing reset to 0.
- **AHI backend** (`audio_io/audio_ahi_live.c`): `SetTaskPri` moves the render task between `AU_LIVE_PRI` 10 and `AU_LIVE_PRI_YIELD` -1. An overloaded song glitches instead of freezing the machine.
- **Heartbeat:** the RIAPP log adds `load=<pm>/1000 overloads=<n>`. A Dell run will confirm or refute the starvation hypothesis.
- **Test:** `t88_live_driver` covers 256 frames at 48 kHz:
  - light load never trips;
  - a sustained 5000 us render trips once;
  - the trip holds 370–377 buffers (2 s) even when load drops;
  - a light-load probe stays normal;
  - heavy load re-trips.
  - 6 of 6 mutants killed.

## Method findings

- **Private VM lane without disturbing others:**
  - reflink-clone an installed AROS disk;
  - boot it with `-netdev user,...,restrict=on`, so the guest agent cannot reach the shared spool port;
  - give it its own HMP port;
  - drive it only through HMP (`screendump`, `sendkey`, `mouse_move`, `mouse_button`).
  - Delete the clone afterwards.
- **HMP input:**
  - Without `usb-tablet`, `mouse_move` steps of 4 px or less map 1:1 (no acceleration).
  - AROS CD caching: swapping the ISO needs `eject -f` plus a new volume label, or the old empty volume stays mounted.
  - Wanderer's Execute dialog (RAmiga+E) keeps old text with the cursor at the start: press `end` plus backspaces before typing.
  - Keystrokes go to the active window; typing into RIAPP toggles play with space.
- **HMP helper speed:** a fixed 0.6 s read wait per HMP command made long pointer paths take minutes; use about 0.05 s for input commands.
- **`pgrep -f` self-match:** `pgrep -f <pattern>` inside a `bash -c` loop matches its own command line and loops forever.
- **Shared build directories:** RIAPP AROS and host builds can run in a `git archive HEAD` copy with `OUT` redirected. This avoids clobbering `/tmp/ri` artifacts another session uses. The full audit writes shared `/tmp/ri` paths, so it was not run while opencode was active.
