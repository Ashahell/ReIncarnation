# T7 — Image decode split (portability plan §3.7)

- Status: GREEN 2026-09-26.
- AROS: `decode_part` + RGBA swizzle moved verbatim from `gui/skin_aros.c`
  to `platform/aros/image_dt.c` as `ri_pal_image_decode`/`ri_pal_image_free`
  (datatypes picture.class, size from `PDTA_BitMapHeader`, 4096 device guard
  kept). datatypes decodes from FILES while PAL takes memory: stages through
  `RI_PATH_TEMP/riimg.tmp` (deleted after; documented in-file).
- `gui/skin_aros.c` keeps manifest/loader/zoom/blitter logic; decode, manifest
  and part path joins go through PAL (`ri_pal_path_join`, `read_whole`
  bounded 8 MB helper, `ri_pal_image_free` for decode-originated buffers).
  Behaviour preserved incl. Classic fallbacks and return codes (manifest
  short-read now returns -2 via `read_whole`, same as the old `got <= 0` path;
  the old 16 KB manifest cap is kept).
- Host: `platform/host/image_host.c` (libpng → core 0xAARRGGBB, 8192 cap).
  §8.1 decision still open: libpng is host-test-only (already the mkskin
  toolchain dep — no new package); Windows still needs WIC/stb (T11).
- Divergence pinned: host cap 8192 vs AROS 4096 — knob strips (45x4480)
  decode on host CI but fall back to Classic on device. Owner call to unify.

## Tests

- `tests/unit/t87_pal_image.c` (RED: `red-t87.txt` — segfault from
  setjmp-clobbered locals + 4096 cap rejecting 4 knob strips):
  every `skins/808-RI` PNG decoded via PAL, dims + full pixels cross-checked
  against an independent simplified-API libpng read; spots `bg02 (4,4) =
  (0x17,0x14,0x10,255)` (mkskin 808 lacquer) and `knob_45x70 (32,35) =
  (0x2f,0x2b,0x27,255)` (mkskin frame-0 body).
- GREEN: `PASS pal_image` (24 files, ≥10 + both spots pinned).
- setjmp lesson: locals modified between `setjmp` and `png_error`'s longjmp
  must be `volatile` (observed: exit 139 under -O2); fixed in `image_host.c`.
- Mutant: R/B swizzle swap → pixel FAILs from index 0 (killed).
- AROS: `skin_aros.c` + `image_dt.c` compile clean (v1 SDK, `-Werror`;
  two real findings fixed: missing `datatypesclass`/`alib_protos` includes,
  missing `__DATATYPES_LIBBASE`); RISECT + RIAPP re-link with `image_dt`
  (218288 / 334960 bytes, 0 UND, 0 r12 — see audit).
