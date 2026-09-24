# 2026-09-24 — Spike lane wedges: giant-send root cause + chunked agent

> Source: server-log forensics (3 wedges) + agent source review +
> Dell device runs
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New (cross-repo: fix lives in Vulkan4Aros). Three `ui-capture`
wedges had one signature; evidence cornered it; fix deployed;
hammer verdict recorded below.

## Evidence (server log, all three sessions)
- Partial capture reply then stall: 436128, 679344, 320792 of
  786521 bytes → client timeout → next job ConnectionReset →
  session ends, agent redials alone (~40 s, sessions 7→11 seen).
- 786521 B = base64(RGB24 512×384) + envelope: every wedge is a
  scale-2 full-screen capture. Hundreds of small frames: zero
  wedges. Chunked bulk puts (≤71 KB): zero wedges.
- Session 5 wedged on its FIRST big frame post-reboot → NOT the
  documented QEMU-e1000 cumulative bug (different path, real
  Intel NIC). Cumulative theory dead by that one fact.
- Agent survives (pre-connect observed; self-redials). No guru
  ever: not an app crash, a transport stall + RST.

## Root cause (best available)
Single ~786 KB `Send()` syscall chokes AROSTCP mid-transfer
(intermittent, timing-dependent). Everything else on the lane is
small or chunked. Length math reviewed clean + guarded (reply
builder), identical sizes usually succeed → not a length bug.

## Fix (Vulkan4Aros `arostcp_shared.h`, guest-side only)
- `atcp_send_all` caps each `Send()` at 32 KB (`ATCP_SEND_CHUNK`):
  same bytes, same order, smaller syscalls. Small frames take the
  byte-identical path they always did.
- Agent rebuilt v11 (117,664 B, 0 UND), deployed with
  backup-first: SYS:ATCPBIN.BAK = original 106,072 B; plain `Copy`
  lands (CLONE variant silently did not); `Compare` + `List`
  verified 117,664 B ---rwed.
- One hypothesis, minimal diff. Host Python untouched.

## Hammer verdict (CORRECTED — overstated, see session-13 note)
- 12 consecutive scale-2 captures post-fix: 12/12 PASS, session 11
  stable. At the time this read as verification.
- Session 13 (same chunked agent) wedged on its 3rd big capture
  with the IDENTICAL partial-then-reset signature (299384/786521).
  So chunking did NOT fix it; at best it may have lowered the rate
  (3 wedges/~25 pre vs 1/~16 post — not significant). Status:
  deployed mitigation, NOT a proven fix. The "verified" claim above
  stands struck.
- Separate deterministic landmine found en route: `Version
  <ELF-binary>` hangs/kills the agent task 3/3 (BAK kills
  identically — new binary exonerated; output capture is capped,
  so not overflow). Avoid until root-caused; agent self-restores.

## Files
- Vulkan4Aros: `src/vulkan/loader/arostcp_shared.h` (chunk cap)
- Dell: SYS:ATCPBIN (new), SYS:ATCPBIN.BAK (original)
- wiki (here): this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
