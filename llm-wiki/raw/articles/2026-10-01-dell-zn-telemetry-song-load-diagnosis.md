# Dell Zombie Nation telemetry, song-load diagnosis, lane connectivity (2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (commit `aa3542c`, pushed)
- Collected: 2026-10-01
- Published: 2026-10-01
- Related: [2026-10-01-menu-hang-tab-artifacts-load-governor.md](2026-10-01-menu-hang-tab-artifacts-load-governor.md), [2026-10-01-dell-deploy-abiv11-usb-stick-layout.md](2026-10-01-dell-deploy-abiv11-usb-stick-layout.md)

## Dell telemetry: Zombie Nation (ABIv11 build of `445cb02`)

- **Run:** `Vk4aros:ReIncarnation/RIAPP SONG=Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng`, 151 bars at 140 BPM, played to the end, then `RIAPP stop`.
- **Timing basis:** 256-frame buffers at 48 kHz, so the period is 5333 us.
- **Heartbeat (one line per ~2840 buffers):**
  - load 175–180/1000 in quiet sections, 295–588/1000 in busy ones;
  - render_max 1114 us at first, then 3175–3372 us;
  - overloads=0 throughout; the governor never tripped.
  - xruns=11, all in one interval between buffers=36528 and 37684. That interval overlapped agent log reads; cause not pinned down.
  - Agent ping stayed 10–27 ms throughout.
- **Demo song on the Dell, loaded from the Songs menu:** load 452/1000, render_max 2668 us, xruns 0.
- **Compared with a private ABIv1 QEMU VM on the host (AMD Ryzen 7 9800X3D, KVM):**
  - load 127–375/1000 on the same songs;
  - render_max 3530–5406 us (one buffer over the period).
  - The Dell is roughly on par with the VM, not ~4x slower as the host-`songplay` estimate assumed.
  - Starvation from Zombie Nation is ruled out on the Dell. The menu-hang record keeps it only as a hypothesis for heavier loads.
- **Draw diagnostics:** the per-redraw timing lines read `draw: full_max=0 ... n=0` throughout, so this run has no UI timing.
- **Raw log:** saved on the stick as `Vk4aros:ReIncarnation/zn-telemetry-20261001.log`.

## "Doesn't load a new song" (owner report)

- The old build logged load failures only to `RAM:RIAPP.LOG`. A failed pick or load showed nothing on screen. The log of the failing session was lost to a Dell reboot.
- **Ruled out by tests:**
  - `SONG=` from the stick loads on the Dell.
  - In the ABIv1 VM, Songs → Load Song... works when stopped and while another song plays (stop → load → play, `snd` changes to the new song).
  - `asl.library` is auto-opened in the ABIv11 link (`libasl.a(asl_autoinit.o)` defines `AslBase`).
- **Fix `aa3542c` (`app/riapp.c`):**
  - The Songs menu logs every selection (`RIAPP songs menu <n>`).
  - `song_pick` logs `cancelled`, `no file chosen (drawer ...)` or `picked <path>`.
  - Song, playlist and requester failures show an `EasyRequestArgs` box: "Cannot load <what> / <path> / <reason>".
- **Dell verification:**
  - A bad `SONG=Vk4aros:nope.rbng` shows the box; the log line reads `open failed`.
  - The owner's next menu load logged `songs menu 0`, then `picked Vk4aros:ReIncarnation/songs/demo/riapp-demo.rbng`, then `16 bars at 140 BPM`, then `play`, and about 27 s later `stop`.
- **Why it can look like nothing loaded:** a single song stops by itself after its last bar plus one bar of tail. The 16-bar demo lasts about 27 s.
- Previous build kept as `Vk4aros:ReIncarnation/RIAPP.prev`.
- `EasyRequestArgs` on this SDK takes `RAWARG` (not `RAW_ARG`). With `%s` only, an `IPTR` array works on x86-64.

## Lane connectivity after a host reboot

- **NordVPN blocks the Dell:** with NordVPN connected (Firewall on, LAN Discovery off), the Dell agent cannot reach `192.168.1.81:9292`.
  - Owner fix: `nordvpn set lan-discovery enabled`. This adds 192.168.0.0/16 to `allowlist_subnets`. ufw already allows 192.168.1.60 on 9292.
- **Diagnosis order:**
  1. `ss -tn state established | grep :9292` (no session);
  2. the ufw rule counters (packets 0);
  3. a 40 s AF_PACKET sniff for 192.168.1.60 (0 packets).
  - The agent was not dialling at all. An agent restart on the Dell fixed it; the new session is `session 1`.
- After the Dell reboots, RAM: is empty (no `RIAPP.LOG`). Start RIAPP from the stick.
- **No mouse hold over the spike agent:** `--ui-click` sends a whole press+release, and there is no button-down/up action. So menus (hold RMB, release over an item) cannot be driven over the spike agent.
  - Menu tests on the Dell need the owner, or a new ui-input action in `Vulkan4Aros/scripts/spike_server.py` (server-side; needs a server restart).
- `--ui-rawkey 0x44` (Return) did not dismiss an EasyRequest. A `--ui-click` on its OK gadget did. The gadget sits about 100 px below the box top at 1366x768.

## Audit in isolation (method)

- `ri_audit.sh` needs a git checkout (add-timestamps step) and `../Vulkan4Aros` beside it. It also writes `/tmp/ri/...`, shared with other sessions.
- **Isolated recipe:**
  1. `git worktree add --detach <scratch>/wt HEAD`;
  2. symlink `<scratch>/Vulkan4Aros`;
  3. sed `/tmp/ri` to a scratch path in `scripts/*.sh` AND in `tests/unit/*.c` (t6, t21, t27, t30, t32, t56, t59, t86, t89, t90 hardcode it). Missing the tests gives a false `FAIL: file-vs-live differ`.
  4. Remove the worktree afterwards.
- Result for `aa3542c`: `AUDIT 0/0 PASS`.

## Owner requirement (sound lanes)

- The Dell (ABIv11) does not need sound. The QEMU ABIv1 lanes have none audible.
- At least one guest must have working sound. Candidate: the riaudio lane (intel-hda + hda-duplex on a host PulseAudio or wav backend, proven earlier).
- Build ABIv11 for the Dell, ABIv1 for QEMU.
