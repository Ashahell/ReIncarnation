/* gui/panelctl.h — front-panel value change -> control plane (§12.11 G9b Step 2).
 * Pure C, host-tested. One path to the engine for every value control on
 * a canvas: reg_id (section << 8 | index, the canvas last-hit vocabulary)
 * -> lane key via ri_ctlreg_auto_id -> ri_ctl_send. The AROS panel calls
 * this (never ri_ctl_send directly); pattern edits and transport use
 * their own pure routes (banks/track, au_live_request).
 */
#ifndef RI_PANELCTL_H
#define RI_PANELCTL_H
#include <stdint.h>

struct RIControlPlane;

/* One canvas value change. Returns 0 sent (exactly one message queued),
 * 1 nothing sent (unknown reg_id, or the control has no engine delivery
 * and its key is 0, or the key was refused), 2 bad args (NULL plane).
 * value is clamped to 0..127. */
int ri_panel_ctl_send(struct RIControlPlane *ctl, uint16_t reg_id, int value);
/* An explicit lane key (Levi per-oscillator params 0x0F.., fidelity P2):
 * sent when allow-listed. 0 sent, 1 refused/full, 2 NULL. */
int ri_panel_ctl_send_key(struct RIControlPlane *ctl, uint16_t key, int value);
struct RISectLevi;
/* Drain hook for the Levi adoption below: move queued messages to the
 * engine (render a buffer in RIAPP, apply events in host tests). */
typedef uint32_t (*ri_panel_drain_fn)(void *dctx);
/* Leviasynth startup adoption (W2, bug 1: the panel shows values the
 * engine is not running at startup). Send every Levi panel value through
 * the bridge so the engine adopts the panel: every keyed registry row at
 * the panel's current value, then every live encoder slot on every module
 * page (all 8 oscillators for the OSC module). The mapping is the live
 * one (ri_slevi_ctl_key / ri_slevi_ctl_idx + value, same as a knob turn):
 * no second mapping to rot. Chunked: drain() runs whenever RI_CTL_CAP/2
 * messages are pending and once more at the end, so the 256-entry plane
 * never overflows. Module/page/opsel are restored on return. Returns the
 * number of messages queued. NULL panel/plane/drain sends nothing. */
uint32_t ri_panel_levi_adopt(struct RISectLevi *s, struct RIControlPlane *ctl,
    void *dctx, ri_panel_drain_fn drain);
struct RISectUI;
/* MIDI value push (M2): send every registry value control (knob, fader,
 * switch, selector, display) whose panel value differs from its shadow
 * byte, then refresh the shadow. Keyless controls never send (same as a
 * live turn). shadow holds nsec * 256 bytes; index [i * 256 + idx].
 * push returns messages queued; shadow_init fills from current values
 * (no sends) and returns controls covered. NULL-safe (0/0). */
uint32_t ri_panel_midi_push(struct RISectUI **uis, const uint8_t *sections,
    uint32_t nsec, struct RIControlPlane *ctl, uint8_t *shadow);
uint32_t ri_panel_midi_shadow_init(struct RISectUI **uis,
    const uint8_t *sections, uint32_t nsec, uint8_t *shadow);
#endif
