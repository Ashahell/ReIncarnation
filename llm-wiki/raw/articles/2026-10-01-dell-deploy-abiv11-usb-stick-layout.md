# Dell deploy: ABIv11 build trap + RIAPP and songs on the USB stick (2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (Dell E6320 deploy after owner reboot; HEAD `445cb02`)
- Collected: 2026-10-01
- Published: 2026-10-01
- Related: [2026-10-01-menu-hang-tab-artifacts-load-governor.md](2026-10-01-menu-hang-tab-artifacts-load-governor.md), [2026-09-30-songs-playlists-zombie-nation.md](2026-09-30-songs-playlists-zombie-nation.md)

## Location on the USB stick (persistent across reboots)

- The stick is volume `Vk4aros:` on device `USBSCSI0P1:` (30.0G VFAT). RAM: is wiped by every reboot.
- `Vk4aros:ReIncarnation/RIAPP` holds the ABIv11 build of HEAD `445cb02` (776608 bytes). It includes the tab-redraw fix and the render load governor (`fe36b9d`).
- `Vk4aros:ReIncarnation/songs/local/demos.rbpl` is the private playlist: Zombie Nation, then the RIAPP demo. The `local/` content never goes to git (public repo).
- `Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng` is the private Zombie Nation song (10090 bytes).
- `Vk4aros:ReIncarnation/songs/demo/riapp-demo.rbng` (5764 bytes) and `songs/demo/demos.rbpl` are the committed demo song and its playlist.
- The playlist entries are relative paths (`zombie-nation/...`, `../demo/...`), so the `songs/local` + `songs/demo` layout must stay together.
- Start command:

  ```
  Vk4aros:ReIncarnation/RIAPP PLAYLIST=Vk4aros:ReIncarnation/songs/local/demos.rbpl
  ```

- The stick root also holds `vulkan4aros/`, `readme.txt`, `RIAPP-EV.LOG` (RIAPP's ev-log volume probe picks `Vk4aros:` first) and `DRIVER-LOGS.txt`.
- The log is `RAM:RIAPP.LOG` (PAL TEMP path is `RAM:`). It is lost on reboot.

## ABIv11 trap (the Dell is ABIv11)

- `scripts/ri_build_aros.sh riapp` builds against the **v1** build-pc SDK (riqemu1 / ABIv1 lane, library base in rdx).
- That binary on the Dell crashed at startup before the first log line. The result was a "Software Failure!" requester, no `RAM:RIAPP.LOG`, and the process left in Status.
- The same binary ran fine in an ABIv1 VM.
- **The Dell recipe** (ABIv11, r12 convention):
  - toolchain: `src/abi/v11/toolchain-core-x86_64/x86_64-aros-gcc`;
  - SDK: `src/abi/v11/sdk/Developer`;
  - compiler flags: `-std=gnu99 -O2 -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -fno-stack-protector -DPCF_TABLE_VERIFIED=1 -Wa,-W`;
  - the riapp source list is taken from the build script;
  - link: `-nostartfiles -no-pie` with `$SDK/lib/startup.o -lamiga -lmui -lintuition -lgraphics -lutility -ldos -lexec -lautoinit -lcybergraphics -lcamd`;
  - check: zero UND symbols.
- **Crash requester handling:** click Suspend (`--ui-click l,590,531` on the 1366x768 screen), never Reboot (owner-only).
- **Capture limits:** `--ui-capture` at scale 1 is refused ("screen too large"). At scale 2 the requester text is unreadable.

## Dell state after the deploy

- The owner rebooted the Dell. The agent answers again (`e6320`, session 9).
- RIAPP started from RAM. The window "RIAPP live panel" is 1349x680 on the 1366x768 screen.
- Log lines at start:
  - `909 pack: 11 voices bound`;
  - `AHI low-level mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us`;
  - zoom mode=1 (1.5x requested, clamped to zoom 0);
  - View menu built;
  - 5 tabs.
- The heartbeat `load=`/`overloads=` fields will confirm or refute the render-starvation cause of the menu hang.
- `tar` is not available on the fresh Dell boot (`--put-tree` extraction failed with "object not found"). Files went over one by one with `--put`, then to the stick with `Copy ... ALL CLONE`.
