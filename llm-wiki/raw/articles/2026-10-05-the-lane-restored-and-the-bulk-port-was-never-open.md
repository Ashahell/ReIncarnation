# The lane restored: the bulk port was configured, claimed, and never open (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, host bridge, Dell E6320)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-which-section-and-why-the-lcd-is-a-floor.md)
- Related: [Dell lane bridge recovery, reboot-proof spooler](../raw/articles/2026-09-27-dell-lane-bridge-recovery.md), [driving the Dell lane by script](../raw/articles/2026-10-02-dell-lane-scripted-ab-procedure-and-click-map.md), [which section owns the build](2026-10-05-which-section-owns-the-build-and-why-the-lcd-is-a-floor.md)

After a host cold reboot the Dell lane was dead: the guest pinged at 0.13 ms, the
bridge was listening, the pairs file was valid, and every job timed out queued.

## "Listen to all ports" uncovered a real defect

The bridge unit declared **no `--bulk-port`**, so it inherited the 9092 default that
another lane's unit already claims — and that unit's journal reads:

```
OSError: [Errno 98] Address already in use
```

**One of the two lanes was left with no bulk listener at all.** It matters here
because `BULK_MIN` is 1 000 000 bytes and an RIAPP binary is 1 014 432 — **over the
line**, so every binary push uses the bulk channel.

**A configured flag is not an open port.** Now `--bulk-port 9192` explicitly, with a
ufw rule from the Dell's address, and proven end to end:

```
[bulkget] Vk4aros:RIAPP.LOG -> DISC1.LOG  34725/34725 B, 1318 ms  OK (sha verified)
```

## And a correction to my own first reading

I took the absence of 9192 from `ss -ltn` as a second fault. It is **correct
behaviour** — `_bulk_listener` binds per transfer, on demand. **I had inferred a fault
from an absence that is the design.**

## The helpers were in /tmp, and /tmp is wiped every reboot

The push and get helpers had been destroyed by every host reboot — eight losses
documented — against a repo rule recorded on 2026-09-27: *git holds everything
load-bearing, `/tmp` only regenerables*. They now live in the repo in **`lane/`**, and
the directory is the point:

- **not `scripts/`**, which is gated to hold exactly five shared scripts
  (`test "$(ls scripts | wc -l)" = 5`). The gate is right: these are lane plumbing for
  one machine, not shared build infrastructure.
- **not `tools/`**, which is gated against hard-coded Amiga paths, and the `RAM:` in
  the launch form would trip it. **A real constraint, not one to route around.**

**The audit passed without being touched.**

## Method, added to the standing set

*Restore what is load-bearing into the repo the moment it is lost twice.* The rule
already existed; what was missing was acting on it. **A rule that is written down and
not applied is a caution carried forward unexamined** — the same shape as the
`r12moves` belief, which sat in the wiki for weeks before anyone checked what it
counted.
