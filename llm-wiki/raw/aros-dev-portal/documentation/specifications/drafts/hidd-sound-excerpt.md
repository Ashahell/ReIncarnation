# HIDD sound-system spec excerpt

**Ingested:** 2026-09-20 into ReIncarnation `llm-wiki`
**Source:** `Vulkan4AROS/llm-wiki/raw/aros-dev-portal/documentation/specifications/drafts/hidd.md` lines 3245-3344
**Provenance:** verbatim excerpt of the sound-HIDD draft section; full document stays in Vulkan4AROS.

## Amiga with internal sound (subclass of hiddclass)

There are two possibilities of how to handle it. The first is to not handle
it, so that the method is passed on to the hiddclass (which doesn't know this
method either) and then on to the rootclass, which returns 0 (FALSE) on
unknown methods. The second possibility is to implement it in soundhiddclass
and return FALSE immediately (because we know that the internal Amiga
soundsystem can't handle MIDI).

## Amiga with sound-card on Zorro-bus (subclass of zorroclass)

The sound-card passes all methods through to its superclass, except methods
for playing/receiving/whatever music. It can either implement them totally
on its own or might use some features of its superclass, for example a general
method for sending data to a Zorro-card.

## Amiga with internal sound or soundcard and additional MIDI-card

This configuration would have two HIDDs, one for MIDI only and one for sound
in general. For the implementation of the last one see above (either Amiga
with internal sound or Amiga with sound-card). The MIDI HIDD-class could
subclass the general sound class (without knowing if it is capable of playing
MIDI) and pass on all methods except MIDI relevant methods. It would fully
overload these.
Another solution would be to subclass hiddclass directly and ignore every
non-MIDI sound command. While the general class would be unit 0, the MIDI
class would be unit 1, so that an application can choose between the normal
sound-system (either the internal Amiga soundsystem or a sound-card, which
might have MIDI capabilities on its own) or the MIDI card. If the MIDI class
would subclass the normal soundhidd, it would feature non-MIDI sound too (by
passing the methods on). Of course, it would have to pass a query for a
HIDDA_Capabilities attribute on to the superclass, so that the capabilities
of the superclass would be recognized by the application for unit 1, too.

## PC with soundblaster-super-ultra-pro-whatever

This HIDD could(!) subclass a class, which handles soundblaster-cards in
general, i.e. the functions that are common to all soundblaster-card (which
itself could subclass something like a pcbusclass). Normally this class would
pass all methods on to its superclass, but it could implement some methods on
its own or partly overload some methods, where this specific soundblaster-card
had advantages/different features than the other soundblaster-cards.

## Some thoughts about the sound.hidd

- An attribute HIDDA_Capabilities [..G], which could define things like:

- HIDDV_Sound_MIDI - sound-system is able to play MIDI sounds (possibly by
using an external MIDI device, such as a keyboard)

- HIDDV_Sound_SFX - sound-system is capable of playing simple sounds (e.g.
the internal pc-speaker)

- HIDDV_Sound_Music

- HIDDV_Sound_Speech
