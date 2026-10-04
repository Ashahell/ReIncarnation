# A 48 kHz uncorked stream coexisted with the guest's 44.1 kHz one — verbatim, 2026-10-04

**Ingested:** 2026-10-04 into ReIncarnation `llm-wiki`
**Source:** host `pactl list sink-inputs`, taken while riqemu1's RIAPP held the
sound card.
**Provenance:** verbatim command output. **This is an unattributed observation,
not a finding** — see section 3.
**Recorded in:** [the AC97 resample record](../articles/2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md)

## 1. What was observed

```
Sink Input #403
	Format: pcm, format.sample_format = "\"s16le\""  format.rate = "44100"  format.channels = "2"  format.channel_map = "\"front-left,front-right\""
	Corked: no
		application.name = "riqemu1"
		module-stream-restore.id = "sink-input-by-application-name:riqemu1"
Sink Input #440
	Format: pcm, format.sample_format = "\"float32le\""  format.rate = "48000"  format.channels = "2"  format.channel_map = "\"front-left,front-right\""
	Corked: no
```

Two sink inputs, both uncorked, at **different rates**: the guest's `riqemu1`
stream at **44100 / s16le**, and a second at **48000 / float32le**.

## 2. Why it is worth keeping

The open question on this lane is *"where does the 44100 come from, given that
nothing asks for it"* — bounded to QEMU's audio back end, with no command-line
knob, because:

- the guest's AHI readback is 48000 (`got=48000`, `nfreq=1`),
- AROS's `ac97` driver hardcodes 48000 and **never writes a codec rate register**,
- QEMU's AC97 model defaults the codec to 48000 (`0xbb80`) and takes its rate from
  that unwritten register,
- and **no `-audiodev` backend accepts a `frequency`** — `pa`, `alsa`, `pw`,
  `none`, `sdl`, `coreaudio` and `jack` all reject it.

This is the **one host-side datum suggesting 48000 is reachable at all on this
machine**: a 48 kHz uncorked stream existed while the guest's 44.1 kHz one ran
alongside it.

## 3. Why this is not a finding

**The second stream is unattributed.** Its `application.name` was not captured —
the grep that produced this output took only `Sink Input`, `Corked`,
`application.name` and `format.*`, and `application.name` did not appear in
`#440`'s stanza. So it is not established whose stream it was.

**It is not the guest.** Three reasons, all of which must be stated together:

- its sample format is `float32le`, while every QEMU `pa` stream observed on this
  host has been `s16le`;
- there were other lanes live at the time — an `nvkcard` VM belonging to another
  session, plus its own audio work;
- and by the time a full listing was taken the count was **0**, so the stream did
  not survive as a stable fixture and cannot be re-inspected.

**A 48 kHz stream on the host says nothing about what the guest's AC97 path can
be made to do.** It is consistent with the resample being avoidable by some means
not yet identified; it is equally consistent with an unrelated application.

## 4. What would settle it

Re-run with the guest's sound card held, and capture the **full** stanza for every
sink input rather than a four-field grep:

```
pactl list sink-inputs
```

specifically `application.name`, `application.process.binary`, `media.name` and
`application.id` for each. That names the stream, and only then is the 48000
observation either promoted to evidence or dropped.

## 5. Standing caveat, unchanged

None of this touches the bound already recorded: the effective 44100 is produced
by neither the AC97 reset default (48000), nor the AROS driver (48000), nor any
command-line option (none exists), so it is in QEMU's audio back end, upstream of
this project. **The fix for the 8.1 % slow playback remains unimplemented and
unavailable from the launch line.**