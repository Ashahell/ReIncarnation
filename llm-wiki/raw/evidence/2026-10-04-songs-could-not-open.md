# Songs could not open: a static path, a flat join, and ASL's dot padding — verbatim, 2026-10-04

**Ingested:** 2026-10-04 into ReIncarnation `llm-wiki`
**Source:** Dell E6320, ABIv11, `-O0`, three successive builds, plus riqemu1 agent
probes.
**Provenance:** verbatim log lines, agent transcripts and `dir` output. Derived
values show their components.
**Recorded in:** [songs could not open: a static path, a flat join, and ASL's dot padding](../articles/2026-10-04-songs-could-not-open.md)

## 0. The report

Owner, mid-session:

> riapp fails to open songs. on riqemu1, songs are on vk4aros reincarnation songs

then, correcting that guess:

> vk4aros only exists on the Dell

Both halves were needed, and the correction matters: `Vk4aros:` is the **Dell's own
stick**, so a single static path could never serve both lanes.

## 1. The three defects, as the log shows them

One run of `Vk4aros:ReIncarnation/songs/` region, `RIAPP.LOG` lines 172-174
(first fix deployed, probe only):

```
RIAPP song Vk4aros:ReIncarnation/songs/zombie-nation.rbng: open failed
RIAPP demo song zombie-nation.rbng not found; built-in demo only
```

Lines 91-92, before any fix — note the path is `SYS:`, not the stick:

```
RIAPP song SYS:Classes/ReIncarnation/Songs/zombie-nation.rbng: open failed
RIAPP demo song zombie-nation.rbng not found; built-in demo only
```

Line 101 — the file requester:

```
RIAPP songs menu 0
RIAPP Load song: picked Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng.......................................................................................
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng.......................................................................................: open failed
```

**Defect 1 — `RI_PATH_SONGS` was a fixed string.** `platform/aros/fs_aros.c`:

```c
case RI_PATH_SONGS: s = "SYS:Classes/ReIncarnation/Songs/"; break;
```

It named a path that exists on riqemu1 and does not exist on the Dell. Proved by
`dir` on each lane:

```
# riqemu1
[exec] 'dir SYS:Classes/ReIncarnation/Songs' -> rc=0 (9754 ms)
         zombie-nation.rbng

[exec] 'dir SYS:Classes/ReIncarnation' -> rc=0 (7 ms)
            Mods (dir)
            Songs (dir)
```

```
# Dell
[exec] 'dir SYS:Classes/ReIncarnation/Songs' -> rc=20 (51 ms)
       Could not get information for SYS:Classes/ReIncarnation/Songs
       object not found
```

**Defect 2 — the library root is not the song's directory.** The probe fixed the
root; the flat join still missed. `dir` on the Dell:

```
[exec] 'dir Vk4aros:ReIncarnation/songs' -> rc=0 (63 ms)
            local (dir)
            demo (dir)

[exec] 'dir Vk4aros:ReIncarnation/songs/local' -> rc=0 (58 ms)
            zombie-nation (dir)
            the-knife (dir)
         demos.rbpl                       knife.rbpl
         riapp-demo.rbng

[exec] 'dir Vk4aros:ReIncarnation/songs/local/zombie-nation' -> rc=0 (63 ms)
         zombie-nation.rbng
```

So the song is two levels below the root, and `root + leaf` can never name it.

**Defect 3 — ASL pads the file name with dots.** Line 101 is not a buffer
overrun. `fr_File` holds the file gadget's *text*, and a partial name is padded
out to the pattern with dots (Amiga ASL behaviour), so the string handed to
`AddPart` is unusable. Source, `filereqhooks.c`:

```c
    /* Save file string gadget text in fr_File */
    if (ifreq->ifr_Flags2 & FRF_DRAWERSONLY)
        name = "";
    else
        GetAttr(STRINGA_TextVal, udata->FileGad, (IPTR *)&name);
    if (!(req->fr_File = VecPooledCloneString(name, NULL, intreq->ir_MemPool, AslBase)))
        goto bye;
```

## 2. The layout, from the wiki

Already recorded, and the fix follows it exactly:

```
Vk4aros:ReIncarnation/songs/local/demos.rbpl
Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng   (10090 bytes)
Vk4aros:ReIncarnation/songs/demo/riapp-demo.rbng                      (5764 bytes)
```

with the constraint that the two directories must stay together:

> The playlist entries are relative paths (`zombie-nation/...`, `../demo/...`),
> so the `songs/local` + `songs/demo` layout must stay together.

## 3. The proof, in one line

Second fix deployed, same lane, same command (`Run RAM:RIPP-SF2`, no argument):

```
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation.rbng: open failed
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng: 151 bars at 140 BPM
```

**151 bars at 140 BPM — the demo song now loads on its own, with no argument and
no requester.** The flat join misses (first line) and the descent finds it
(second), which is the whole mechanism visible in two lines.

## 4. Two errors of mine, on the record

**I claimed an explicit `SONG=` did not win. It did.**

```
  9:RIAPP playlist Vk4aros:ReIncarnation/songs/local/knife.rbpl: 1 songs
 10:RIAPP song Vk4aros:ReIncarnation/songs/local/the-knife/the-knife.rbng: 104 bars at 124 BPM
 ...
 23:RIAPP song RAM:zombie-nation.rbng: 151 bars at 140 BPM
```

Line 23 is my run and it loaded. **The log appends across runs**; I read the
first `RIAPP song` line in the file, which belonged to an earlier session. A
conclusion drawn from "the first line says X" is worthless here — anchor on the
last run.

**My space injection used the wrong scancode.** `gui/keymap.h`:

```c
#define RI_RAW_SPACE 0x40u
```

0x40 is **64**. I injected **57**. So on this lane no spacebar ever reached
RIAPP, and every "playback started after the key injection" claim is
unsupported — RIAPP auto-plays anyway, which is why songs played regardless.
`--ui-rawkey` takes the same raw codes as `RI_RAW_*`, so the correct invocation
is `--ui-rawkey 64`.

## 5. Two lane facts this run established

**A crashed RIAPP survives `--ui-close`, and Q (0x10) does not stop it either.**

```
[exec] 'status' -> rc=0 (22 ms)
       Process 8 Loaded as command: RAM:RIPP-SONGFIX
       Process 9 Loaded as command: RAM:RIPP-SF2

[ui  ] close '#0' [closerequest] ok (44 ms)
[ui  ] close '#1' [closerequest] ok (45 ms)
[ui  ] injected 1 event(s)          # Q, scancode 0x10 = RI_KM_QUIT

[exec] 'status' -> rc=0 (64 ms)
       Process 9 Loaded as command: RAM:RIPP-SF2
```

Window close returned `ok` three times and the process stayed. This is how a
**second RIAPP got launched alongside the first** — the AHI contention and the
interleaved log followed from it.

**An instance can come up in offline-render-only and it is silent.** The
successful run logged its draw lines and `RIAPP play`, and nothing else:

```
RIAPP play
RIAPP draw: full_max=4130 us ...
RIAPP draw: full_max=0 us ...
RIAPP draw: full_max=0 us ...
```

No `RIAPP stg:`, no `dstg`, no `vcount`, no `hb:`. Every one of those is behind
`if (s_live && s_lv.drv.session)`, so their **total absence** means offline mode
— it does not mean the engine was idle and it does not mean the counters
broke. A heartbeat that logs draw lines but no stage lines is this, not a
silent renderer.