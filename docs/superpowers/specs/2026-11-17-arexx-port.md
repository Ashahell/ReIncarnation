# ARexx Port Design Note (Part X, X0)

> Written 2026-11-17. Owner approval required before X1.
>
> **Status 2026-11-21 (advisor review): NOT owner-approved; X1–X2 were built
> without that stop.** Implementation corrected (see llm-wiki
> `2026-11-21-review-of-wiring-census-and-arexx-port.md`). EXPORTWAV (§5) is
> **not** implemented in RIAPP: it replies rc 20; an offline render on the
> main task would share the live session with the audio driver. Open
> question 2 is answered by that, pending owner.

---

## 1. Port Name

**Spec says:** `ADDRESS REINCARNATION` (primary), `REINCARNATIONAROS` (deprecated alias).

**Zune's behaviour:** With `MUIA_Application_Base "RIAPP"` and `MUIA_Application_UseRexx`/`Commands`, Zune creates port `RIAPP.1` (not `REINCARNATION`). Zune's command table prefix-matches and does not reliably check ReadArgs failure.

**Decision:** RIAPP creates its **own public `MsgPort` named `REINCARNATION`** using `Forbid(); FindPort(); AddPort(); Permit();`, refusing to start if the name is taken. Keeps `REINCARNATIONAROS` only as a parser alias (handled in `arexx.c`). Does **not** use Zune's `MUIA_Application_UseRexx`/`Commands`. Leaves `MUIA_Application_Base` alone.

**Rationale:** The spec and manual document `ADDRESS REINCARNATION`. A Zune-managed port would be `RIAPP.1`, breaking scripts. Our parser is exact-string; Zune's prefix-matching would cause conflicts. Own port = full control.

---

## 2. Command Set v1

### Existing commands (from `arexx.c` / `arexx_dispatch.c`):
| Command | Args | Reply (rc 0) |
|---------|------|--------------|
| `PLAY` | — | `OK PLAYING` |
| `STOP` | — | `OK STOPPED` |
| `OPENSONG` | `<path>` | `OK OPEN <path>` |
| `EXPORTWAV` | `<song> <wav>` | `OK EXPORT <song> <wav>` |
| `SETRPPARAM` | `<ctl> <val>` | `OK PARAM <ctl> <val>` |

- `ctl`: 0..65535 (registry `reg_id`)
- `val`: 0..127
- Optional leading port token `REINCARNATION` / `REINCARNATIONAROS` stripped by parser.

### Recommended additions:
| Command | Args | Reply (rc 0) | Notes |
|---------|------|--------------|-------|
| `SETCTL` | `<reg_id> <value>` | `OK PARAM <reg_id> <value>` | Any registry row via `ri_panel_ctl_send`, range-checked against row. ARexx is one more door, not a back door. |
| `GETCTL` | `<reg_id>` | `OK VALUE <reg_id> <value>` | Returns current panel value (0..127). |
| `TEMPO` | `<bpm>` | `OK TEMPO <bpm>` | Sets song tempo via `ri_live_set_bpm` (20..500). |
| `STATUS` | — | `OK STATUS <PLAYING\|STOPPED> <bpm> <song>` | Transport state + current tempo + song name. |
| `QUIT` | — | `OK QUIT` | Clean shutdown (posts quit to main loop). |

### `SETRPPARAM` compatibility:
- Kept for compatibility. Maps to `SETCTL` internally.
- 0..127 contract stays. `reg_id` 0..65535.
- Reply format `OK PARAM <ctl> <val>` identical.

### Error replies (rc 10 = bad arg, rc 12 = bad cmd, rc 20 = failed to act):
- All malformed/unknown commands get rc 10/12 with `ERROR ...` text.
- Every message is replied, including malformed ones.
- Messages still queued at exit are replied with rc 20 before `RemPort`.

---

## 3. Threading

**All commands run on the MAIN TASK** in the existing `Wait()` loop (`app/riapp.c` ~5034):
```c
sigs |= SIGBREAKF_CTRL_C | timer_bit;
sigs = Wait(sigs);
```

The ARexx port signal (`mp_SigBit`) is added to this mask. The loop drains with `GetMsg` → `IsRexxMsg` → `ARG0` → `ri_arexx_handle` → effects → `ReplyMsg`.

**Never on the audio path.** Every effect goes through the same functions the GUI uses:
- `ri_core_play` / `ri_core_stop`
- `ri_core_load_song`
- `ri_live_set_bpm`
- `ri_panel_ctl_send` (for `SETCTL`/`GETCTL`)

So the panel, lamps, and automation see every ARexx change.

---

## 4. Replies

- `rm_Result1` = rc (0/10/12, plus 20 for "failed to act")
- `rm_Result2` = `CreateArgstring(text)` **only** when `RXFF_RESULT` is set in the message
- Every message is replied, including malformed ones
- Messages still queued at exit are replied with rc 20 before `RemPort`

---

## 5. EXPORTWAV

**Decision:** Offline render through the **existing render path** (`tools/render.c` logic via `ri_engine_render`), **not deferred**.

**Reason:** The render tool already supports `--song --out --depth --rate --format` and produces bit-identical output to the live engine (proven by t116/t37). Reusing this path:
- Guarantees the exported WAV matches what the user hears
- Supports all depths (16/24/32) and formats (WAV/AIFF)
- No separate render engine to maintain
- Can run synchronously on the main task (blocking but acceptable for a script command)

**Implementation:** `EXPORTWAV` loads the song (if not current), runs the render loop to completion, writes the file, replies `OK EXPORT ...`. For long songs this blocks the main loop; a future version could defer to a background task, but v1 keeps it simple and synchronous.

---

## 6. Files to Modify / Add

| File | Change |
|------|--------|
| `project/arexx_aros.c` | Complete the port loop: open `rexxsyslib.library`, create `REINCARNATION` port, add signal to wait mask, drain loop with `GetMsg`/`IsRexxMsg`/`ri_arexx_handle`/`ReplyMsg` |
| `project/arexx_dispatch.c` | Split into pure reply contract (keep `t1_rel` pinned strings byte-exact) + effect interface (function pointers the app fills in) |
| `project/arexx.h` | Add new command codes (`RIAREXX_SETCTL`, `RIAREXX_GETCTL`, `RIAREXX_TEMPO`, `RIAREXX_STATUS`, `RIAREXX_QUIT`) and effect function pointer typedefs |
| `app/riapp.c` | Add port signal to wait mask (~5034), fill effect function pointers, call drain loop |
| `scripts/ri_build_aros.sh` | Add three ARexx TUs (`arexx.c`, `arexx_dispatch.c`, `arexx_aros.c`) to RIAPP link list; keep out of host build (audit gate ~1532 pattern); keep `#error` guard |
| `tests/unit/tNNN_arexx_effects.c` | New host test: asserts `PLAY` calls play, `SETCTL` calls `ri_panel_ctl_send` with right `reg_id`/value, out-of-range refused, unknown command calls nothing |
| `lane/rxsend/` | Tiny AROS sender probe (like `lane/blitprobe`): `CreateRexxMsg` + `CreateArgstring`, `RXCOMM \| RXFF_RESULT`, `FindPort("REINCARNATION")` under `Forbid`, `PutMsg`, wait reply, print rc and RESULT |

---

## 7. Testing Strategy

### X1 (host):
- `tNNN_arexx_effects` asserts effect function pointers are called correctly
- `t1_rel` continues to pin reply strings byte-exact

### X2 (AROS compile):
- RIAPP builds v1 with 0 unresolved and 0 r12 moves
- `ri_audit.sh` green

### X3 (Dell/riqemu1 proof without interpreter):
- `lane/rxsend` proves every v1 command, reply text = host-pinned string
- `PLAY`/`STOP` change transport AND its lamp (owner looks or ev-log)
- `SETCTL` on one row per section lands (census row turns `OK` for D12)
- Malformed/unknown command → rc 10, RIAPP keeps running
- RIAPP quit with message queued → sender gets rc 20, no hang
- Second RIAPP refuses port name cleanly
- Then add D12 to census (W0 table) and re-run W3 gate

### X4 (Regina - owner's call):
- If yes: build Regina from AROS contrib for ABIv1, ABI-check, stage `RexxMast` + library, reboot (owner), prove `rx "ADDRESS REINCARNATION; 'STATUS'; say result"` + one script per command
- If no: record port proven by `lane/rxsend` only, update manual's `ARexx` node

### Manual update:
- `docs/ReIncarnation.guide` node `ARexx`: final command set, port name, reply codes, one example script

---

## 8. Open Questions for Owner

1. **Port name collision:** If another app creates `REINCARNATION` port first, RIAPP refuses to start. Acceptable?
2. **EXPORTWAV blocking:** Synchronous render blocks main loop. Acceptable for v1?
3. **Regina staging:** Build Regina from contrib for ABIv1? (Owner's call per X4)
4. **GETCTL scope:** Return panel value (what user sees) or engine value (what engine runs)? Recommend panel value for consistency with `SETCTL`.

---

*End of design note. Owner approval required before X1 implementation.*