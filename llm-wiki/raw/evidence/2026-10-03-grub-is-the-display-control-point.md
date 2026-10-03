# The riqemu1 display mode is a GRUB kernel argument, not an Intuition preference — verbatim, 2026-10-03

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** host-side investigation after the owner corrected the diagnosis;
guest files pulled over the agent channel from `DH0:boot/grub/`.
**Provenance:** verbatim agent output, verbatim config lines with line numbers,
verbatim file sizes and diffs.
**Recorded in:** [the display mode is a GRUB argument, and grub.cfg.mine is a deliberate backup](../articles/2026-10-03-the-display-mode-is-a-grub-kernel-argument.md)

## 1. The starting observation

The guest booted at 1024x768 long after boot, where it had previously been
1280x1024:

```
P6 1024 768 255
```

Two earlier boots in the same day had reported:

```
P6 1280 1024 255
```

## 2. First hypothesis, and why it was wrong

`SYS:Prefs/ScreenMode` turned out to be an ELF binary, not a data file:

```
$ bulkget SYS:Prefs/ScreenMode
[bulkget] SYS:Prefs/ScreenMode -> /tmp/opencode/ScreenMode  69112/69112 B, 9789 ms  OK (sha verified)
$ file ScreenMode
ScreenMode: ELF 64-bit LSB relocatable, x86-64, version 1 (AROS Research Operating System), not stripped
```

`DEVS:Monitors/VMWare` likewise:

```
$ file VMWare.monitor
VMWare.monitor: ELF 64-bit LSB relocatable, x86-64 ...
```

There was **no stored preference at all**:

```
$ dir ENVARC:Sys
         font.prefs                       GL.default
         iconset.var                      nv_location
         pointer.prefs                    theme.var

$ bulkget ENVARC:Sys/screenmode.prefs
[bulkget] ENVARC:Sys/screenmode.prefs -> /tmp/opencode/screenmode.prefs  REFUSED: bulk_get_begin failed: cannot open file
```

**Owner's correction:** *"we had to select 1280x ourself in the grub menu, which is
before any intuition prefs"*. That is right, and it invalidates the whole
Intuition-level line of enquiry.

## 3. What actually selects the mode

`DH0:boot/grub/` contains:

```
$ dir DH0:boot/grub
         x86_64-efi (dir)
         fonts (dir)
         i386-efi (dir)
         i386-pc (dir)
         locale (dir)
         roms (dir)
         grub.cfg                         grub.cfg.mine
         grub.cfg.orig                    iso9660_stage1_5
         menu.lst                         menu.lst.DH0
         splash.png                       stage1
         stage2                           stage2_hdisk
```

`menu.lst` shows the mechanism plainly — the mode is a **kernel argument chosen by
menu entry**:

```
31:title AROS64 with VESA Gfx @ 1024x768-8bpp
32:    kernel /boot/pc/bootstrap.xz vesa=1024x768x8 ATA=32bit nomonitors
40:title AROS64 with VESA Gfx @ 1280x1024-8bpp
41:    kernel /boot/pc/bootstrap.xz vesa=1280x1024x8 ATA=32bit nomonitors
67:title AROS64 with VESA Gfx @ 1024x768-16bpp
68:    kernel /boot/pc/bootstrap.xz vesa=1024x768x16 ATA=32bit nomonitors
76:title AROS64 with VESA Gfx @ 1280x1024-16bpp
77:    kernel /boot/pc/bootstrap.xz vesa=1280x1024x16 ATA=32bit nomonitors
103:title AROS64 with VESA Gfx @ 1024x768-32bpp
104:    kernel /boot/pc/bootstrap.xz vesa=1024x768x32 ATA=32bit nomonitors
112:title AROS64 with VESA Gfx @ 1280x1024-32bpp
113:    kernel /boot/pc/bootstrap.xz vesa=1280x1024x32 ATA=32bit nomonitors
```

In `grub.cfg` the same two entries appear as:

```
28:menuentry "AROS64 with native Gfx" {
29-    multiboot2 /boot/pc/bootstrap.xz ATA=32bit $bootstrap_flags debug=serial

149:menuentry "AROS64 with VESA Gfx @ 1280x1024-32bpp" {
150-    multiboot2 /boot/pc/bootstrap.xz vesa=1280x1024x32 ATA=32bit nomonitors  $bootstrap_flags
```

**The native entry passes no `vesa=` argument at all**, so the bootstrap
auto-detects and the mode varies per boot.

## 4. The live config defaulted to native; only the backup was pinned

```
$ grep -nE "^set default|^set timeout|gfxmode|gfxpayload" grub.cfg
16:    	set gfxmode=640x480
23:set timeout=5
26:set default="AROS64 with native Gfx"
194:    set gfxpayload=text
205:    set gfxpayload=text
```

The pin existed only in `grub.cfg.mine`, which GRUB does not load. Sizes and
identity before any edit:

```
  grub.cfg             7132 bytes  md5=a9218fcc8435
  grub.cfg.mine        7186 bytes  md5=748319472098
  grub.cfg.orig        7050 bytes  md5=1faf544f1fec
```

and the complete difference between the live config and the backup:

```
$ diff grub.cfg grub.cfg.mine
26c26
< set default="AROS64 with native Gfx"
---
> set default="AROS64 with VESA Gfx @ 1280x1024-32bpp"
213a214
> 'set default AROS64 with native Gfx '
```

`grub.cfg.mine` also carried the comment explaining the intent:

```
25:# riqemu1: default pinned to VESA 1280x1024x32 (the native entry lets the VBE mode vary per boot)
26:set default="AROS64 with VESA Gfx @ 1280x1024-32bpp"
```

Line 214 is **malformed** — stray single quotes, outside any block, so GRUB raises
a parse error rather than honouring it.

## 5. This was a deliberate change, not a lost edit

The wiki record for 2026-09-30 already explains the one-line diff, and gives a
reason that matters:

> **Root cause:** GRUB pinned `vesa=1280x1024x32` **plus `nomonitors`** (both in
> the default entry's `ARGS:` per `ShowConfig`); `nomonitors` keeps every monitor
> driver out, so the mode walk finds exactly **1 mode (1280x1024x24)**.
> DEVS:Monitors ships VMWare/NVidia/IntelGMA/ATI but none can load.
>
> **Fix (lane, reversible):** `DH0:Boot/grub/grub.cfg` default →
> `"AROS64 with native Gfx"` (backup `grub.cfg.mine`; one-line diff, byte-verified
> round trip). Native boot ARGS: `ATA=32bit debug=serial`.
>
> **Result:** 23 modes, all avail, incl. 1366x768, 1600x900, 1680x1050,
> **1920x1080 (0x00101000)**, 1920x1200, 2560x1600/1440.
>
> **Owner step:** pick the mode in `Prefs/ScreenMode` (persists in ENVARC).
> Deliberately NOT forced at boot (a `vesa=1920x1080` single-mode entry needs
> unproven VBE support — bricking risk, SFS unwritable from host).
>
> Lane state parked (2026-09-30): GRUB default = native (wide-capable); backup
> `grub.cfg.mine` on DH0.

So the pin costs the lane its wide-mode capability, and the documented mechanism
for an owner-chosen persistent mode is **ENVARC**, not GRUB.

## 6. The change applied

```
grub.cfg line 26: set default="AROS64 with native Gfx"
                  -> set default="AROS64 with VESA Gfx @ 1280x1024-32bpp"
grub.cfg.mine line 214: removing malformed "'set default AROS64 with native Gfx '"

after both edits, grub.cfg == grub.cfg.mine : True
```

Deployed, and read back byte-identical:

```
[put ] /tmp/opencode/grub.cfg.fixed -> DH0:boot/grub/grub.cfg  7148 B in 2 chunks, 40 ms  sha_ok=True written=7148 OK
[put ] /tmp/opencode/grub.cfg.mine.fixed -> DH0:boot/grub/grub.cfg.mine  7148 B in 2 chunks, 31 ms  sha_ok=True written=7148 OK
[bulkget] DH0:boot/grub/grub.cfg -> /tmp/opencode/grub.cfg.back  7148/7148 B, 6 ms  OK (sha verified)
=== grub.cfg line 26 on the guest ===
set default="AROS64 with VESA Gfx @ 1280x1024-32bpp"
=== matches what I sent? ===
  IDENTICAL
```

## 7. Removing the Intuition prefs file

It was written first, on the wrong hypothesis, and pins **depth 8** where GRUB
now requests **x32** — leaving it would make Intuition downgrade the depth GRUB
just chose. There is no way to delete a guest file with what is available: the
guest Shell accepts only `Run`, `SetEnv`, `echo`, `dir`, `status`, `wait`,
`Break`, `Getenv`, and the agent has no delete verb. Hence a purpose-built
tool:

```
[exec] 'RAM:rmfile ENVARC:Sys/screenmode.prefs ENV:Sys/screenmode.prefs' -> rc=0 (19 ms)
       RM: deleted ENVARC:Sys/screenmode.prefs
       RM: deleted ENV:Sys/screenmode.prefs

$ dir ENVARC:Sys/screenmode.prefs
[exec] 'dir ENVARC:Sys/screenmode.prefs' -> rc=20 (21 ms)
       Could not get information for ENVARC:Sys/screenmode.prefs
       object not found
```

Absence of complaint is not proof; the refusal is.

## 8. Two build traps, both of which masqueraded as something else

**A relative SDK path produced broken shim symlinks**, so the link failed with
`cannot find -lstdlib`:

```
libcrt.a -> ../Vulkan4Aros/src/abi/v1/.../Developer/include/../lib/libstdc.a
resolves to: /tmp/ri/Vulkan4Aros/...   <- nonexistent
```

The repo script absolutises with `cd "$V1SDK" && pwd`; using the path verbatim
stores a relative target that resolves against the shim directory. With an
absolute path:

```
SDK=/home/miller/Work/projects/Vulkan4Aros/src/abi/v1/core-pc-x86_64/bin/pc-x86_64/AROS/Developer/include
resolves to: /home/miller/.../Developer/lib/libstdc.a
shim OK
link exit=0
```

**`head` on a compile pipe killed gcc via SIGPIPE**, so the object file was never
written — and the next command then reported it as a *link* error:

```
[compile piped to `head -6`]  -> /tmp/opencode/rmfile.o: No such file or directory
```

Re-running the compile without a pipe gave `compile exit=0` and a 3088-byte `.o`.
**A missing object file reported by the linker is a compile-stage failure until
proven otherwise.**

Resulting tool, for the v1 lane:

```
-rwxrw-rw- 1 miller miller 24344 rmfile
  OS/ABI:                            AROS
  Machine:                           Advanced Micro Devices X86-64
r12 base moves: 0  (v1 lane => 0)
```

## 9. Verification

Three cold boots, the last two with **no Intuition preference present at all**:

```
P6 1280 1024 255
P6 1280 1024 255
P6 1280 1024 255
[ui  ] windows 1280x1024 screen, 2 window(s)
[ping] -> ok=True (3 ms)
```

Window mapped and rendering:

```
1287 38 1261 1390
mapped True
  window mean luminance: 120.3  max 255.0
```

**Not confirmed:** the depth. The entry requests `vesa=1280x1024x32` and Intuition
inherits the bootstrap's mode, but `screendump` emits RGB regardless, so the depth
cannot be read back from the framebuffer. The 2026-09-30 record notes the single
VBE mode was `1280x1024x24`, so `x32` in the ARGS and `x24` observed is itself an
open discrepancy.