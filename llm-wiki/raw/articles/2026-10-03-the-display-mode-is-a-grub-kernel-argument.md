# The display mode is a GRUB kernel argument, and `grub.cfg.mine` is a deliberate backup (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane, riqemu1 display)
- Collected: 2026-10-03
- Published: 2026-10-03
- Raw: [GRUB is the display control point, verbatim](../evidence/2026-10-03-grub-is-the-display-control-point.md)
- Related: [riqemu1 wide modes, parked audio](2026-09-30-riqemu1-wide-modes-parked-audio.md), [proving the VM window is actually visible](2026-10-03-proving-the-vm-window-is-actually-visible.md), [the AHI probe build contract](2026-10-03-the-ahi-probe-build-contract.md)

Asked to make riqemu1 always start at 1280x, I diagnosed it at the wrong layer,
was corrected, and the correction exposed a decision that had been made
deliberately three days earlier and written down. **The useful content here is
the correction and the trade-off, not the fix.**

## I was wrong, and the wrongness had a shape

The guest was at `P6 1024 768 255` where it had been 1280x1024. I assumed an
Intuition-level cause and went looking for a screen-mode preference — the
documented persistence mechanism for an owner-chosen mode:

```
$ file ScreenMode          # SYS:Prefs/ScreenMode
ScreenMode: ELF 64-bit LSB relocatable, x86-64 ... AROS Research Operating System

$ file VMWare.monitor      # DEVS:Monitors/VMWare
VMWare.monitor: ELF 64-bit LSB relocatable, x86-64 ...

$ bulkget ENVARC:Sys/screenmode.prefs
[bulkget] ENVARC:Sys/screenmode.prefs -> ...  REFUSED: bulk_get_begin failed: cannot open file
```

Both "config" files are binaries, and no preference existed at all. I read that
as "the preference is simply absent, so Intuition falls back to the driver's
nominal dimensions" — built a prefs file from AROS's own writer format, and got
1280x1024 across two cold boots.

**Then the owner said the thing that should have been asked first:**

> we had to select 1280x ourself in the grub menu, which is before any intuition
> prefs

Which is right, and it means the whole enquiry had been one layer too late. **A
boot-time display property is not owned by the layer that draws it.**

## What actually owns it

`DH0:boot/grub/menu.lst`, which is the clearest statement of the mechanism in the
whole system:

```
31:title AROS64 with VESA Gfx @ 1024x768-8bpp
32:    kernel /boot/pc/bootstrap.xz vesa=1024x768x8 ATA=32bit nomonitors
40:title AROS64 with VESA Gfx @ 1280x1024-8bpp
41:    kernel /boot/pc/bootstrap.xz vesa=1280x1024x8 ATA=32bit nomonitors
112:title AROS64 with VESA Gfx @ 1280x1024-32bpp
113:    kernel /boot/pc/bootstrap.xz vesa=1280x1024x32 ATA=32bit nomonitors
```

The resolution is a **kernel argument on the bootstrap**, selected by menu entry.
And the native entry passes none at all:

```
menuentry "AROS64 with native Gfx" {
    multiboot2 /boot/pc/bootstrap.xz ATA=32bit $bootstrap_flags debug=serial
```

No `vesa=`, so the bootstrap auto-detects, **and the mode varies per boot**. That
is the whole explanation for a display that changes between reboots on a
deterministic machine.

## The one-line diff was a decision, not a slip

This is the part worth keeping. The live config defaulted to native while a backup
held the pin, and the two files differed by exactly one line:

```
$ diff grub.cfg grub.cfg.mine
26c26
< set default="AROS64 with native Gfx"
---
> set default="AROS64 with VESA Gfx @ 1280x1024-32bpp"
213a214
> 'set default AROS64 with native Gfx '
```

My first instinct was that the pin had been **lost** — someone edited the live
file, forgot the backup, and the live file drifted. `grub.cfg.mine` even carried a
comment describing the pin as deliberate. But the wiki already had the answer,
from 2026-09-30:

> **Root cause:** GRUB pinned `vesa=1280x1024x32` **plus `nomonitors`**;
> `nomonitors` keeps every monitor driver out, so the mode walk finds exactly
> **1 mode (1280x1024x24)**. … **Fix (lane, reversible):** `grub.cfg` default →
> `"AROS64 with native Gfx"` (backup `grub.cfg.mine`; one-line diff, byte-verified
> round trip). … **Result: 23 modes**, all avail, incl. **1920x1080**.

**So `.mine` is the preserved "before" state, and the one-line diff is the change
itself.** I had been about to revert a deliberate, documented, byte-verified
decision on the strength of a file that is never read.

And the trade-off is real in both directions:

| | `vesa=1280x1024x32` + `nomonitors` (pinned) | `native` (no `vesa=`) |
|---|---|---|
| resolution | **deterministic 1280x1024** | varies per boot |
| modes visible to the walk | **1** (1280x1024x24) | **23**, incl. 1920x1080 |
| monitor drivers | all excluded by `nomonitors` | VMWare/NVidia/IntelGMA/ATI |

The 2026-09-30 note also records a reason for not forcing it harder: *a
single-mode entry needs unproven VBE support — bricking risk, SFS unwritable from
host*. And it names the intended mechanism for the owner's choice:
**`Prefs/ScreenMode`, which persists in ENVARC** — the layer I had independently
reinvented, and then deleted.

## What I changed, and what it costs

`DH0:boot/grub/grub.cfg` line 26 is now pinned, verified byte-identical on the
guest, and `grub.cfg.mine` had its malformed line 214 removed so the two files are
byte-identical and the pin can no longer be lost asymmetrically:

```
after both edits, grub.cfg == grub.cfg.mine : True
```

The Intuition prefs file was **deleted**, not left in place: it pinned depth 8
where GRUB now requests x32, so keeping it would have made Intuition silently
downgrade the depth GRUB had just chosen.

Verified — three cold boots, the last two with **no preference present at all**:

```
P6 1280 1024 255
P6 1280 1024 255
[ui  ] windows 1280x1024 screen
window mean luminance: 120.3  max 255.0
```

**The requirement is met, and the wide-mode capability is not.** Per the table
above, the mode walk should now see one mode rather than 23. That is a lane-policy
trade-off the owner owns, not a call to make silently — see the open question at
the end.

## Deleting a guest file needs a binary

There is no delete anywhere in the available surface: the guest Shell accepts only
`Run`, `SetEnv`, `echo`, `dir`, `status`, `wait`, `Break`, `Getenv`, and the agent
has no delete verb. So removing my own prefs file required a purpose-built AROS
tool, because restoring the pre-existing state means the file should not exist.

```
RM: deleted ENVARC:Sys/screenmode.prefs
RM: deleted ENV:Sys/screenmode.prefs

$ dir ENVARC:Sys/screenmode.prefs -> rc=20
       object not found
```

**Absence of complaint is not proof that a delete worked** — a silent no-op and a
successful unlink look identical from the tool's side. The `object not found`
refusal is the evidence. That asymmetry is worth remembering in the other
direction too: this lane's `bulk_get` on a missing file shouts, while `--get` on
an *open* file whispers stale bytes.

## Two build traps that impersonated other problems

**A relative SDK path silently broke the library shim.** The `libcrt.a` /
`libstdlib.a` / `libcrtprog.a` symlinks were created from a relative path, so they
resolved against the shim directory and pointed nowhere:

```
libcrt.a -> ../Vulkan4Aros/src/abi/v1/.../Developer/include/../lib/libstdc.a
resolves to: /tmp/ri/Vulkan4Aros/...        <- nonexistent
```

The link failed with `cannot find -lstdlib`, which reads like a missing
dependency rather than a dangling symlink. The repo script absolutises with
`cd "$V1SDK" && pwd`; using the path verbatim is the trap.

**`head` on a compile pipe killed gcc with SIGPIPE**, so the object file was never
written — and the next command reported it as a *link* failure:

```
[compile piped to `head -6`]  ->  rmfile.o: No such file or directory
```

Re-run without a pipe: `compile exit=0`, 3088-byte `.o`. **A missing object file
named by the linker is a compile-stage failure until proven otherwise** — the
message points at the wrong stage.

## Open

**The depth is unconfirmed.** The entry requests `x32`; the 2026-09-30 record
observes the single VBE mode as **1280x1024x24**. `screendump` emits RGB
regardless, so depth cannot be read back from the framebuffer, and the `x32` vs
`x24` discrepancy is itself unexplained. A HIDD/`GfxBase` probe would settle it.

**The wide-mode trade-off is unresolved and is the owner's.** Deterministic
1280x1024 was requested; it has been delivered at the cost of a 1-mode walk. The
alternative that satisfies both is the documented one — restore the native entry
for the full mode list, and persist the chosen 1280x1024 in ENVARC via
`ScreenMode` prefs, which is what I had built and then removed. That needs a depth
decision, because a prefs file has to name one.

## Method

- **Ask which layer owns the property before instrumenting the layer that draws
  it.** "Before any Intuition prefs" was in the owner's first sentence.
- **A file that exists but is never read is worse than a missing file** — it looks
  authoritative and carries the answer you want. `grub.cfg.mine` looked like the
  source of truth and was in fact a backup of a superseded state.
- **Diff a config against its siblings before editing either.** One line, and the
  wiki already explained why.
- **Verify a delete by refusal, not by silence.**

## See Also

- [riqemu1 wide modes, parked audio](2026-09-30-riqemu1-wide-modes-parked-audio.md) — the deliberate decision this reverts, and the `nomonitors` trade-off
- [proving the VM window is actually visible](2026-10-03-proving-the-vm-window-is-actually-visible.md) — the `P6` header instrument, and why it settles the wrong question
- [the AHI probe build contract](2026-10-03-the-ahi-probe-build-contract.md) — `Printf`, `DOSBase`, and the other ABI-layer traps hit while building the unlink tool