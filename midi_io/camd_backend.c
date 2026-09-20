/*
 * camd_backend.c — CAMD MIDI bridge (Task 13, gate G13).
 *
 * AROS-ONLY. Bridges camd.library + realtime.library (spec §13/W1
 * platform facts: pluggable DEVS:MIDI/ drivers, Poseidon
 * camdusbmidi.class/Bulk for USB-MIDI) into the pure learn map in
 * midi_io/midi.c (host-tested). Must NEVER enter the host build: the
 * #error below fires on any non-AROS compile,
 * scripts/ri_build_host.sh never references this file, and
 * scripts/ri_audit.sh gates both facts (probe_ahi.c precedent).
 *
 * Bridge contract (mirrors the host stub state machine in midi.c):
 * hot-unplug mid-playback = no crash, no hang: the bridge times out
 * the device (RI_MIDI_TIMEOUT), pending MIDI events for it are dropped
 * and counted, transport continues (TC-2.8.4). Flood shed at
 * RI_MIDI_MAX_PER_BUFFER oldest-first (P-19 OPEN value).
 */

#ifndef __AROS__
#error "camd_backend.c is AROS-only: CAMD bridge, never in the host build"
#endif

#include <exec/types.h>
#include "midi_io/midi.h"

/* Device handle owned by the bridge task (opened on DEVS:MIDI/
 * selected driver at transport start, closed at stop). NULL = no
 * device (RI_MIDI_NO_DEVICE); a dead handle after unplug reads as
 * RI_MIDI_TIMEOUT until replug. Full open/pump wiring is Task-14 app
 * wiring; this TU owns the state mapping + the per-buffer pump
 * discipline so the audit can compile-check it. */
static APTR ri_camd_cluster = NULL;
static LONG ri_camd_dead = 0;

LONG ri_camd_status(void) {
    if (ri_camd_dead)
        return (LONG)RI_MIDI_TIMEOUT;
    if (!ri_camd_cluster)
        return (LONG)RI_MIDI_NO_DEVICE;
    return (LONG)RI_MIDI_OK;
}

/* Hot-unplug entry: mark the handle dead (bridge task times out the
 * device), drop pending events for it (counted), transport continues. */
void ri_camd_unplug(void) {
    ri_camd_dead = 1;
    midi_backend_unplug();
}

/* Per-buffer pump: take at most RI_MIDI_MAX_PER_BUFFER bytes, shed
 * oldest-first via midi_flood_push (counted in f->dropped), stamp
 * against the master clock at the caller. Playback timing never
 * stalls on MIDI (§17 failure 4). */
void ri_camd_pump(struct RIFlood *f, const UBYTE *bytes, ULONG n) {
    ULONG i;
    if (!f || ri_camd_dead)
        return;
    for (i = 0; i < n; i++)
        midi_flood_push(f, bytes[i]);
}
