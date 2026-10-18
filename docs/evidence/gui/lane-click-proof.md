# Lane click + key injection: acceptance script (Dell E6320, ABIv11)

Owner-runnable proof that injected pointer/key events reach RIAPP on real
hardware. Written 2026-10-18 against the bounded-exec agent
(`SYS:ATCPBIN`, 144,752 B, md5 `1bb124840131e0afb70ba3a03f48025a`).
Every coordinate below was re-grounded from a capture on this build —
do not reuse numbers from another build.

## 0. Lane state

One agent per pair. Two pairs exist so proof traffic never fights the
owner's session:

| pair | port | agent | spooler |
|---|---|---|---|
| `e6320` | 9292 | `SYS:ATCPBIN` (persistent) | `spike-laptop.service` |
| `e6320x` | 9294 | `RAM:ATCPX` (scratch) | `spike-s6.service` |

```bash
SP="python3 ~/Work/projects/Vulkan4Aros/scripts/spike_server.py submit --spool /tmp/spike_spool_laptop"
```

The agent must be started by hand after every reboot
(`Run SYS:ATCPBIN agent 192.168.1.81 9292 e6320`); it never self-redials.
Health check that distinguishes the three common failures:

- `--ping` answers but every `--exec`/`--ui-*` is refused with
  `proto>=2` → the agent predates the bounded exec lane, rebuild it
  (`ATCP_EXTRA_CFLAGS="-DATCP_BOUNDED_EXEC=1"`), do not debug the lane.
- `--ping` silent → the agent's command lane is wedged; it cannot
  self-recover, the owner restarts it.
- no session at all → nobody dialed; check the pair name against
  `spool/sessions/*.json` (a typo is refused with `unknown-name`).

## 1. Coordinate map (1366x768 guest, capture scale 2)

| target | guest coords | note |
|---|---|---|
| tabs row | y = 167 | Synths 70, Drums 130, PAT 194, FX ~222, Mix 250 |
| device rail (power) | y = 128 | x = 33 toggles `dev=2` (808) on the Drums page |
| 808 CUT OFF knob | (420, 256) | drag target for `CTL` |
| neutral panel spot (focus) | (700, 200) | inside the 808 panel, no control |

The first click on an inactive window only activates it — send the
click twice when switching pages from a cold window.

## 2. Tab switch

```bash
$SP --wait 300 --ui-click l,130,167 --exec "wait 2" --ui-click l,130,167 \
    --exec "wait 3" --exec "Search Vk4aros:RIAPP-EV.LOG TAB"
```

Pass: `TAB page=1 us=22774 … sec=4 art=4 bay=11 built=56`. A cold page
costs ~21-28 ms (MUI rebuild); a repeat click costs ~25 us (`kept=`).

## 3. Rail power toggle

```bash
$SP --wait 300 --ui-click l,33,128 --exec "wait 3" \
    --exec "Search Vk4aros:RIAPP-EV.LOG VIS"
```

Pass: `VIS dev=2 show=0 mask=1b` then `show=1 mask=1f`. The render
follows the mask (the 808 strip disappears/returns) — capture to confirm,
a log line alone proves the handler ran, not the redraw.

## 4. Knob drag

```bash
$SP --wait 300 --ui-press l,420,256 --exec "wait 1" \
    --ui-move 420,280 --ui-move 420,310 --exec "wait 1" \
    --ui-release l,420,310 --exec "wait 2" \
    --exec "Search Vk4aros:RIAPP-EV.LOG CTL"
```

Pass: a `CTL <cc>=<value>` line (`021b=8` on this build).
`--ui-move` takes `x,y` only — no button prefix; `--ui-press/--ui-release`
take `button,x,y`. Mixing the forms aborts the job with a `ValueError`
after the press has already been delivered.

## 5. Skin cycle (Ctrl+M on the focused section)

```bash
$SP --wait 420 --ui-click l,700,200 --exec "wait 2" \
    --ui-rawkey 0x37,q0x8 --ui-rawkey 0x37,up,q0x8 --exec "wait 3" \
    --exec "Search Vk4aros:RIAPP-EV.LOG SKIN"
```

Pass: **exactly one** `SKIN sync=1 … current=<name>` per press, and the
four presses walk the installed set and return:

```
SKIN sync=1 current=808-RI
SKIN sync=1 current=Template
SKIN sync=1 current=Stale
SKIN sync=0 current=Classic
```

Press and release must be back-to-back in one job. A press without its
release is held, the keyboard repeats at ~3 Hz, and one logical press
becomes hundreds of cycles. Shift must not be held (`Shift+Ctrl+M` sets
the whole panel instead of the focused section).

## 6. Reading the log

`Vk4aros:RIAPP-EV.LOG` is the event log (TAB/VIS/CTL/SKIN), the session
log is `RAM:RIAPP.LOG`. Close a log before fetching it — an open file
locks and `get` fails.

## 7. Traps that cost real time

- **Directory `get` wedges the agent.** `--get SYS:` never returns and
  the agent stops answering `ping`; only an owner restart recovers it.
  Use plain file `get`/`put`; enumerate with `exec "List …"`.
- **RAM: does not survive.** A reboot or a wedge-restart clears it;
  re-upload and hash-verify before trusting a guest binary.
- **`RAM:` wipes between the put and the run** if the machine resets in
  between — verify with a `get` round trip, not with the put's own
  success message.
- **Two agents on one pair displace each other** every ~30 s. Drain with
  `--bye`; `Break` will not kill an agent.
- **The guest's own input is indistinguishable from yours** in the log
  when you share the keyboard. Only attribute lines that your job's
  window pins (compare the event counter before and after).