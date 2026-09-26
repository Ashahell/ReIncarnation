# T1 — Atomics and SPSC hardening (portability plan §4 T1)

- Status: GREEN 2026-09-26.
- Header: `platform/pal/ri_pal_thread.h` — `ri_atomic_u32` / `ri_atomic_ptr`,
  `ri_atomic_load_acq` / `ri_atomic_store_rel` / `ri_atomic_fetch_add_rel`,
  ptr variants. GCC/Clang lower to `__atomic` acquire/release; MSVC uses
  `_ReadWriteBarrier` (x86-64 TSO; ARM64 Windows needs a real fence — T11).
  Includes only `<stdint.h>` / `<stddef.h>` (audit-gated).
- Migrated (acquire/release; init plain-store before sharing, commented):
  - `engine/seq/ctlplane.h/.c` head/tail;
  - `engine/seq/autolane_emit.h`, `engine/seq/autolane.c` `RIAutoPub.front/staged`;
  - `engine/seq/riseq.h/.c` `snap`/`pending` (`ri_atomic_ptr`);
  - `engine/live.h/.c` `meters_seq` (seqlock ticket);
  - `audio_io/audio_ahi_live.h/.c` `xruns`/`buffers`/`render_us_max`/`render_us_sum_ms`,
    `cmd`/`state`, `cap_pos`/`cap_on`, plus `s_open_gen`/`s_hook_count`
    (the two `s_open_gen.v++` / `s_hook_count.v=` sites now use
    `fetch_add_rel` / `store_rel`; the `lv` init block keeps plain stores
    with a sharing comment).
  - `app/riapp.c` log line reads via `ri_atomic_load_acq`.
- Left as-is: `audio_io/probe_ahi.c s_player_ticks` (AROS-only probe),
  `AuPlayEx(..., const volatile int *stop)` API (caller flag, not a PAL word),
  `engine/fx/pcf.c rows[i].v` (pattern value, not atomic — name collision only).

## Tests

- `tests/unit/t84_pal_thread.c` (RED in `docs/evidence/portability/red-t84.txt`,
  GREEN after the header + migrations):
  - atomic round-trips (u32 + ptr);
  - message-passing: release store publishes payload to acquire load
    (200k pings, two threads);
  - SPSC ring under threads: 50k msgs, FIFO order, no loss;
  - publish + snapshot + meter seqlock smoke.
- `bash scripts/ri_build_host.sh test t84_pal_thread` → `PASS pal_thread`.
- Regression: t80/t81/t83 still PASS.
- TSan: `gcc -fsanitize=thread` build of t84 runs clean (`PASS pal_thread`,
  exit 0, no warnings).
- Mutant: replace the first `__atomic_store(..., RELEASE)` in the PAL header
  with a plain `a->v = v`; TSan reports `data race ... in ri_atomic_load_acq`
  (producer vs consumer on `t_flag`), proving the barrier is load-bearing.
  Functional test still passes on x86-64 TSO (expected — the bug is ARM64 /
  compiler-reorder, caught by TSan, not by value checks).

## Audit

- `scripts/ri_audit.sh`: added `t84_pal_thread`, PAL include gate
  (`stdint.h`/`stddef.h` only), and `no volatile in engine/` gate.
- Full `scripts/ri_audit.sh` before commit (see commit body for result).
