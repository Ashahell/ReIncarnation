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
#include "gui/skinsect.h"

struct RIPanelUI {
    uint8_t focus;                              /* RI_FOCUS_* */
    uint8_t pad[3];
    struct RIKeyOpts opts;
    struct RISectUI *synth[2];                  /* RI_SEC_SYNTH1/2 */
    struct RISectUI *drum[2];                   /* RI_SEC_808 / RI_SEC_909 */
    struct RISectUI *pat[RI_FOCUS_COUNT];       /* RI_SEC_PAT_* */
    struct RISectUI *tr;                        /* RI_SEC_TRANSPORT */
    struct RISectUI *mix[6];                    /* RI_SEC_MIX_* + RI_SEC_MASTER + RI_SEC_MIX_LEVI (MIDI, G7) */
    struct RISectUI *fx[4];                     /* RI_SEC_PCF .. RI_SEC_COMP (MIDI, G7) */
    struct RIKeyAction last;                    /* last decoded action (readout/proofs) */
    uint16_t last_raw, last_qual;               /* last raw key event seen (diagnostics) */
    /* live state (G6a): playhead step per focus section, -1 = stopped */
    int8_t playhead[RI_FOCUS_COUNT];
    uint8_t playing;
    uint8_t del_held, del_focus;                /* Shift+Tab / Shift+key held (p. 33, 44) */
    int8_t del_arg;
    uint64_t play_start_ticks;                  /* transport cursor when playback began */
    uint64_t play_start_16ths;                  /* audio-clock count at that same edge */
    uint32_t changes;                           /* bumps on every applied change */
    const char *const *skin_installed;          /* app-owned installed mod names */
    uint32_t skin_n;
    char skin_current[64];                      /* selected mod name */
    struct RISkinAssign skin_assign;            /* S7 per-section mods */
    int8_t tab_req;                             /* keyboard tab request 0..3, -1 none */
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
 * Menu actions toggle the two keyboard options here; RI_KM_SELECT_MOD cycles
 * the installed skin list (ri_panel_skins); the other menu commands are
 * decoded only (p->last). Taps are decoded only (p->last): recording at the
 * playhead belongs to G6. */
int ri_panel_key(struct RIPanelUI *p, uint32_t raw, uint32_t qual);
/* Installed skin list for Ctrl+M cycling (app-owned pointers, copied only
 * the current name). Empty list (n == 0) disables cycling. */
void ri_panel_skins(struct RIPanelUI *p, const char *const *installed,
                    uint32_t n, const char *current);
/* Live feed (G6a): `sixteenths` = 16ths since playback started, from the
 * render task's sample position (gui/livestate.h); playing = transport
 * state. Updates every section's playhead (each loops its own pattern
 * length, p. 147), lets the Song-mode bar display follow, and applies a
 * held delete-tap to each step the playhead reaches (p. 33, 44). Taps
 * (RI_KA_TAP) record at the current playhead and only while playing
 * ("With playback activated").
 *
 * Returns WHY something changed, 0 when nothing visible did, so the
 * caller repaints only what that change can show. The art reads live
 * state in three places, and only these (owner Dell 2026-10-01): the
 * 808/909 chase lamps (gui/draw/art_shared.c ri_art_chase, called from
 * art_section.c for the two drum sections only), the focus bar
 * (art_pat.c, moved by clicks, not by the tick) and the transport's
 * Song Position display (gui/secttr.c ri_str_follow). Nothing reads
 * panel->playing: the transport lamps change on the press that moved
 * them. */
#define RI_PANEL_CH_PLAYHEAD 0x1u   /* a focus step moved: the drum lamps */
#define RI_PANEL_CH_PLAYING  0x2u   /* the transport playing edge */
#define RI_PANEL_CH_FOLLOW   0x4u   /* the Song Position display followed */
#define RI_PANEL_CH_TAP      0x8u   /* a held delete-tap edited a step row */
uint32_t ri_panel_live(struct RIPanelUI *p, int playing, uint64_t sixteenths);

/* What those changes make stale on a canvas that draws `section`. The
 * sets are the evidence above, kept as a table so the 100 ms tick is a
 * dispatch and the finding is a law (t71):
 *  - the drums answer STEPS (their own 16-step rows, repainted old and
 *    new); no other section draws a chase lamp;
 *  - the transport answers BAR (its Song Position display);
 *  - the playing edge is stale nowhere;
 *  - a delete-tap edited pattern data, so only a section with step keys
 *    can show it: the 808, the 909 and the Levi rows (the 303 patterns
 *    have no on-panel steps). t71 pins that set against the registry. */
#define RI_STALE_NONE  0
#define RI_STALE_STEPS 1
#define RI_STALE_BAR   2
#define RI_STALE_ALL   3
int ri_panel_live_stale(uint32_t mask, uint32_t section);

/* Which caller asked for a box repaint, so the cost of an expensive
 * repaint can be attributed (Dell 2026-10-02: a 303 ms partial and a 4 ms
 * partial are different failures and the aggregate n=/max= pair cannot tell
 * them apart). These sit here, not in rsection.h, because rsection.h is
 * AROS-only and the policy has to be host-testable. Distinct, contiguous,
 * starting at 0, so a bucket array indexed by them is total. */
#define RI_RSEC_BOX_NONE  0  /* unattributed: never charge a caller by guess */
#define RI_RSEC_BOX_STEPS 1  /* drum step lamps (the chase) */
#define RI_RSEC_BOX_BAR   2  /* the Song Position display */
#define RI_RSEC_BOX_OTHER 3  /* any other caller, or a reason out of range */
/* Number of reason codes, i.e. the size of every per-reason bucket array and
 * of the set the heartbeat must print. Dell 2026-10-02: NONE was a legal code
 * that no bucket printed, so a whole reason split reported zeros and looked
 * like "no repaints happened". Print all RI_RSEC_BOX_COUNT of them. */
#define RI_RSEC_BOX_COUNT 4
/* Any code outside the four folds onto OTHER: a new caller cannot index
 * out of bounds, and an unknown reason is visible as OTHER rather than
 * silently charged to one of the known callers. */
static inline int ri_rsection_box_why(int why) {
    return (why >= RI_RSEC_BOX_NONE && why <= RI_RSEC_BOX_OTHER)
        ? why : RI_RSEC_BOX_OTHER;
}
#endif
