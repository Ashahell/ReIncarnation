# F4: write-up and upstream draft (draft only — not sent anywhere)

Date: 2026-10-07. F3 did not run (owner decision pending).

## Answers

- **The type:** the Dell VESA framebuffer (`0xD0000000`, bootloader source)
  is **UC**: no variable MTRR covers it so the BIOS default (UC, MTRRs
  enabled) applies, while the page tables select power-on PAT entry 0 (WB)
  — WB+UC→UC. AROS programs neither MTRRs nor PAT, and the v11 PCI mapping
  adds no caching attribute. Full numbers in `2026-10-07-f1-memtype.md`.
- **The speed:** yes, UC explains 220 MB/s — the same session's BLITPROBE
  baseline reads 480 µs RAM→RAM (4369 MB/s) against 9509 µs RAM→WIN
  (220 MB/s) and 9274 µs RectFill (226 MB/s); the CPU and memory are fine,
  uncached PCI writes are the limit (`2026-10-07-f2-speed.md`).
- **WC:** not measured; no WC trial ran.

## Upstream draft (embargoed: no issue/PR/mail; no project names)

Title: map the VESA linear framebuffer write-combining (MTRR/PAT), or
repaint bandwidth stays at uncached rates.

Body:

> On real hardware with a BIOS MTRR default of UC over the PCI hole, the
> VESA driver's shadow-to-screen copy (`vesagfx_support.c: UpdateRect ->
> CopyMem` to `hwdata->framebuffer`) runs at uncached write speed
> (~220 MB/s measured for a 1024x512 blit; RAM→RAM on the same machine is
> ~4.3 GB/s). The kernel identity-maps the aperture with PWT=0 PCD=0
> (`arch/x86_64-pc/kernel/mmu.c: core_InitMMU`), selecting power-on PAT
> entry 0 (WB), so the effective type is whatever the MTRRs say — and
> nothing programs them (`0x2FF`/`0x277` appear nowhere in `arch/`,
> `rom/`, `workbench/hidds/`). The generic PCI `MapPCI`
> (`rom/hidds/pci/pcidriverclass.c`) is an identity pass-through with no
> caching attribute (the ABIv1 `PCIPC_IDENTITYMAPPED` + `MAP_CacheInhibit`
> override only covers BARs past 128 GiB).
>
> Options: (a) the kernel programs a WC PAT entry at boot (as other OSes
> do) and vesagfx maps the framebuffer through it; (b) vesagfx sets a WC
> variable MTRR when PAT is unavailable. Measured readouts backing this:
> MTRRCAP VCNT=10 FIX=1 WC=1, MTRR_DEF default UC E=1, power-on PAT
> byte-exact, 2 MiB pages with PAT index 0 at base/mid/end of a
> `0xD0000000` framebuffer.
>
> Risks: variable-MTRR overlap rules (UC wins; other overlaps undefined),
> so the range must avoid existing UC carve-outs; identical MTRRs on every
> logical CPU (SMP); CPUs without PAT; interaction with other graphics
> drivers mapping the same aperture.

Whether/when this goes upstream is the owner's decision (prompt §9.2).

## Sent

2026-10-07, on the owner's explicit instruction: filed as
https://github.com/aros-development-team/AROS/issues/1508 (proposal issue,
no code; no project names in the text).

## Review addendum (advisor, 2026-10-07): facts not in issue #1508

These are candidate follow-ups for the issue. Posting them is the owner's
call; nothing has been posted.

- The machine's firmware left a **disabled WC MTRR (MTRR8) staged at the
  framebuffer base** (`0xD0000000`, 64 MiB, V=0, stray mask bit 16). This
  supports option (b), and shows that the range is free of UC carve-outs.
- On Sandy Bridge the IntelGMA driver's device IDs are commented out, so
  `vesagfx` is the only driver on such machines. That makes the
  write-combining change the only lever for them.
- Doing it at runtime needs a CPU rendezvous: the kernel's
  `core_DoCallIPI` exists, but no public LVO. So option (b) belongs in the
  kernel (or in `vesagfx` through a new kernel entry), not in user code.
