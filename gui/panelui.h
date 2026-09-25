/* gui/panelui.h — the whole front panel: focus + keyboard dispatch
 * (§12.10 G5). Pure C, host-tested. Holds pointers to the section states
 * the canvases own (any may be NULL when a proof shows a subset) and the
 * one focus the manual describes (p. 22): the orange bar next to the
 * Pattern selectors of the section that has it. Focus moves by clicking
 * in a section, by selecting a pattern in it (mouse or keyboard), and by
 * the up/down arrow keys.
 */
#ifndef RI_PANELUI_H
#define RI_PANELUI_H
#include <stdint.h>
#include "gui/keymap.h"
#include "gui/sectui.h"

struct RIPanelUI {
    uint8_t focus;                              /* RI_FOCUS_* */
    uint8_t pad[3];
    struct RIKeyOpts opts;
    struct RISectUI *synth[2];                  /* RI_SEC_SYNTH1/2 */
    struct RISectUI *drum[2];                   /* RI_SEC_808 / RI_SEC_909 */
    struct RISectUI *pat[RI_FOCUS_COUNT];       /* RI_SEC_PAT_* */
    struct RISectUI *tr;                        /* RI_SEC_TRANSPORT */
    struct RISectUI *mix[5];                    /* RI_SEC_MIX_* + RI_SEC_MASTER (MIDI, G7) */
    struct RISectUI *fx[4];                     /* RI_SEC_PCF .. RI_SEC_COMP (MIDI, G7) */
    struct RIKeyAction last;                    /* last decoded action (readout/proofs) */
    uint16_t last_raw, last_qual;               /* last raw key event seen (diagnostics) */
    /* live state (G6a): playhead step per focus section, -1 = stopped */
    int8_t playhead[RI_FOCUS_COUNT];
    uint8_t playing;
    uint8_t del_held, del_focus;                /* Shift+Tab / Shift+key held (p. 33, 44) */
    int8_t del_arg;
    uint64_t play_start_ticks;                  /* transport cursor when playback began */
    uint32_t changes;                           /* bumps on every applied change */
};

void ri_panel_init(struct RIPanelUI *p);
/* Focus index a section belongs to (synth, rhythm, its mixer, its pattern
 * section); -1 for transport, master and the FX units. */
int ri_panel_focus_of(uint32_t section);
/* A click anywhere in a section: focus follows when it has a focus index.
 * Returns 1 when the focus changed. */
int ri_panel_click(struct RIPanelUI *p, uint32_t section);
/* A pattern selected with the mouse in a pattern section moves the focus
 * too (p. 22); call after the section handled the click. */
int ri_panel_pattern_selected(struct RIPanelUI *p, uint32_t section);
/* Decode + apply one raw key event. Returns 1 when state changed.
 * Menu actions toggle the two keyboard options here; the other menu
 * commands are decoded only (p->last) — the menu system is G8. Taps are
 * decoded only (p->last): recording at the playhead belongs to G6. */
int ri_panel_key(struct RIPanelUI *p, uint32_t raw, uint32_t qual);
/* Live feed (G6a): `sixteenths` = 16ths since playback started, from the
 * render task's sample position (gui/livestate.h); playing = transport
 * state. Updates every section's playhead (each loops its own pattern
 * length, p. 147), lets the Song-mode bar display follow, and applies a
 * held delete-tap to each step the playhead reaches (p. 33, 44). Taps
 * (RI_KA_TAP) record at the current playhead and only while playing
 * ("With playback activated"). Returns 1 when anything visible changed. */
int ri_panel_live(struct RIPanelUI *p, int playing, uint64_t sixteenths);
#endif
