# M4 lane proof — the Leviasynth on its own channel (2026-10-09)

Staged for the owner to run. Nothing here is a claim until the owner's
report and the ev-log are in.

## What is staged in the Dell's `RAM:`

| file | build | bytes |
| --- | --- | --- |
| `RIAPP.M4` | `2a17e90` (ABIv11, mixed -O2 engine / -O0 app+GUI) | 1129544 |
| `MIDISEND` | CAMD script sender (unchanged since M0) | 34480 |
| `m4-levi.mid.txt` | the script below | — |

## The script, in words

`m4-levi.mid.txt` sends everything on **MIDI channel 2** (the Leviasynth
device) unless a line says otherwise:

1. a held C-E-G chord, then a four-note scale run with rising velocity;
2. CC 55 closed then opened (digital filter cutoff, manual p. 168) —
   listen for dark then bright;
3. CC 74 closed then opened (analog filter cutoff, p. 168);
4. CC 1 the mod wheel 0 → 96 → 0 (a performance signal, not a knob);
5. pitch bend centre → up → centre;
6. channel aftertouch 80 then 0;
7. **the no-collision law, from both sides** — CC 7 on channel 2 (the
   Leviasynth's own master volume: it must move *nothing*, because the
   ReBirth master fader is a ReBirth control), then CC 55 on channel 1
   (same number, remote map: it must move a ReBirth control and must
   *not* change the Leviasynth's cutoff);
8. a note on channel 1, which must not sound a Leviasynth voice.

## How to run it

From a shell on the Dell:

```
RIAPP_DEMO=0 RAM:RIAPP.M4
```

Leave the panel as it comes up. Then, in a second shell:

```
RAM:MIDISEND riapp RAM:m4-levi.mid.txt
```

Stop RIAPP when the script finishes, and tell me. I will pull the
ev-log (`RAM:RIAPP-EV.LOG`) and read the counters.

## What to listen for, and what each line proves

- steps 1–2: notes sound and stop; velocity changes loudness.
- step 3: **audible** dark→bright→dark.
- step 4: **audible** change with CC 74 (a second, different filter).
- step 5: the mod wheel changes the sound and returns to normal.
- step 6: the pitch bends up and comes back.
- step 7: pressure changes the sound; release restores it.
- step 8: **CC 7 does nothing audible and the master fader does not
  move**; **CC 55 on channel 1 does not reopen the Leviasynth's filter**
  (it may move a ReBirth knob — that is the remote's job).
- step 9: **silence** for the channel-1 note.

## The counters I will read

- `LEVI chan remote=1 levi=2 devices=2` at startup — the table bound as
  E0 says.
- one `LEVI note <n> on/off vel <v>` per note event, with matching counts
  on and off;
- one `LEVI perf <k> val <v> hi <h>` per bend/AT/wheel;
- one `LEVI param <key>=<val>` per CC;
- session xruns (`xr=`) — must stay 0.

## Known limits of this proof, stated before it runs

- **It is scripted, not played.** MIDISEND is a message script, so this
  proves the routing and the engine, not a player's feel. Latency is not
  measured here.
- **No USB-MIDI port on this machine**, so a real keyboard is not part
  of this run.
- **`midi_drain()` itself has no host test** — the branch that dropped
  every performance signal until the fix in `2a17e90` was invisible to
  t184. This run is what covers it. Treat a green ev-log as necessary,
  not sufficient.
- Chord mode (`RI_CTL_LEVI_CHORD`) is engine behaviour covered by the
  host tests; this run does not exercise it.
