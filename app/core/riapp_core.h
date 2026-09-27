/* riapp_core.h — portable application core (portability plan T8).
 * Session wiring, demo song, transport and meter publishing for the live
 * app. Pure C99, host-tested; the AROS shell keeps the MUI window, the
 * canvases, the sync shadows and the AHI/null backend choice.
 * All storage caller-owned (the shell holds one static struct).
 */
#ifndef RI_APP_CORE_H
#define RI_APP_CORE_H
#include <stdint.h>

#include "engine/live.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "engine/seq/sched.h"

#define RI_CORE_SCRATCH 512u
/* Shared delay-send line storage (owner 2026-09-27: no live path ever
 * provided one, so delay sends were dry). ~2.7 s at 48 kHz; longer
 * musical delays clamp to the cap inside the FX unit. */
#define RI_CORE_DLINE 131072u

struct RIAppCore {
    struct RIPatternBank banks[4];
    struct RISongTrack track;
    struct RIControlPlane ctl;
    struct RILiveSession session;
    struct RIEvent scratch[RI_CORE_SCRATCH];
    float dline[RI_CORE_DLINE];
};

/* Init banks/track/control plane/session (ppq/sr/bpm/engine as given).
 * Does NOT start transport (shell plays after backend handshake). */
void ri_core_init(struct RIAppCore *c, uint32_t ppq, float sr, float bpm,
    uint32_t engine);
/* Demo song: 16-step 303 line (accents + slides) over an 808 beat. */
void ri_core_demo(struct RIAppCore *c);
/* Bank by instance 0..3 (303A, 303B, 808, 909); out of range -> bank 0. */
struct RIPatternBank *ri_core_bank(struct RIAppCore *c, uint32_t inst);
const struct RIPatternBank *ri_core_bank_ro(const struct RIAppCore *c, uint32_t inst);
/* Transport against the owned session (null-backend path; the AROS shell
 * routes through the render task when the AHI backend is up). */
void ri_core_play(struct RIAppCore *c);
void ri_core_stop(struct RIAppCore *c);
/* Sticky live selection: store sel at (bar, inst) when the grid differs.
 * Returns 1 when stored, 0 when already there (or on bad args). The panel
 * calls this every round so the selection persists as bars advance
 * (E1 pattern mode); arrangement-per-bar programming is a song-mode
 * feature and stays out (OPEN). */
int ri_core_capture_sel(struct RIAppCore *c, uint64_t bar, uint32_t inst, uint8_t sel);
/* Meter snapshot -> levels + song position. Returns 1 when a read landed
 * (shell refreshes on 1), 0 when busy/skipped. Pure computation; the shell
 * pushes the levels into the mixer board + panel. */
int ri_core_meters(struct RIAppCore *c, int *lvl303, int *lvl808,
    uint64_t *sixteenths, uint32_t mix_freq, int playing);

#endif
