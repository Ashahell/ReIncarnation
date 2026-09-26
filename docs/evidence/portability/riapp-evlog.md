# RIAPP event log (owner request, 2026-09-27)

- `RIAPP-EV.LOG`: every user-driven event with `ev <seq> <buffers> <kind>
  <detail>` lines — `CTL reg=val`, `TR PLAY/STOP`, `PAT/PATLEN/PATOFF`,
  `STEP slot/step/key/flags`, plus a `RUN frames= vol=` header.
- Fresh per run (`MODE_NEWFILE` at startup); volume probe order:
  `Vk4aros:` (owner-confirmed Dell stick) → `USB0:/USB1:/UMSD0:/UMSD1:/
  USBDISK0:` → `RAM:` fallback. Open once, `Flush` per line (a wedge keeps
  all but the last line); closed on all exits (incl. the two `return 5`
  paths); best effort if the stick vanishes.
- No host test (AROS-only shell; host build untouched): proven by v1+v11
  `-Werror` compiles, 0-UND links, and the T6 literal gate (which fired
  twice during development — first on `"RAM:"` fallbacks, fixed through
  `ri_pal_path`).
- AROS `vsnprintf` has a non-C99 prototype in the posixc shim — the stamp
  uses a hand-rolled `evlog_putu` + `ri_log_format` instead (recorded so
  the next vsnprintf user doesn't repeat this).
- Proves itself on the next interactive Dell run: `RUN vol=Vk4aros:` must
  appear, and knob drags must append `CTL` lines.
