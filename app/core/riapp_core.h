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
#include "engine/seq/autolane.h"
#include "engine/seq/autolane_emit.h"

#define RI_CORE_SCRATCH 512u
/* Shared delay-send line storage (owner 2026-09-27: no live path ever
 * provided one, so delay sends were dry). ~2.7 s at 48 kHz; longer
 * musical delays clamp to the cap inside the FX unit. */
#define RI_CORE_DLINE 131072u
/* Song automation lane capacity per buffer (songs & playlists, owner
 * 2026-09-30): sound programs at tick 0 plus arrangement moves. */
#define RI_CORE_AUTO_CAP 8192u

struct RIAppCore {
    struct RIPatternBank banks[5];
    struct RISongTrack track;
    struct RIControlPlane ctl;
    struct RILiveSession session;
    struct RIEvent scratch[RI_CORE_SCRATCH];
    float dline[RI_CORE_DLINE];
    /* Song automation (tick-0 sound programs + moves), double-buffered
     * for the render task; empty until a song loads. */
    struct RIAutoEv autoev[2][RI_CORE_AUTO_CAP];
    struct RIAutoPub pub;
    struct RIAutoCarry carry;
    struct RIAutoPass pass;
    uint32_t song_bars;   /* bars up to the last sounding one (0 = open-ended demo) */
    uint32_t song_sections; /* RI_ENGINE_S* devices the song plays */
    float song_bpm;
};

/* A song as loaded (the RBNG codec's view, kept free of project/ types):
 * banks by instance (NULL = empty), track, tempo, automation triples at
 * song ppq. */
struct RICoreSong {
    const struct RIPatternBank *bank[RI_SONGTRACK_INSTANCES];
    const struct RISongTrack *track;
    float bpm;
    uint32_t ppq;
    const uint32_t *auto_tick;
    const uint16_t *auto_ctl;
    const uint8_t *auto_val;
    uint32_t nauto;
};

/* Init banks/track/control plane/session (ppq/sr/bpm/engine as given).
 * Does NOT start transport (shell plays after backend handshake). */
void ri_core_init(struct RIAppCore *c, uint32_t ppq, float sr, float bpm,
    uint32_t engine);
/* Demo song: 16-step 303 line (accents + slides) over an 808 beat. */
void ri_core_demo(struct RIAppCore *c);
/* Load a song (stopped transport; caller stops first): banks, track,
 * tempo and automation replace the current ones. Returns 0 ok, 2 bad
 * args / automation refused (the lane stays empty then). Sets
 * song_bars (bars through the last bar where any instance plays a
 * non-empty pattern) and song_sections. */
int ri_core_load_song(struct RIAppCore *c, const struct RICoreSong *s);
/* 1 when the pattern plays nothing (303: every step a rest; drum/Levi:
 * no hits). Pure. */
int ri_core_pattern_silent(const struct RIPatternBank *b, uint32_t slot);
/* Bank by instance 0..3 (303A, 303B, 808, 909); out of range -> bank 0. */
struct RIPatternBank *ri_core_bank(struct RIAppCore *c, uint32_t inst);
const struct RIPatternBank *ri_core_bank_ro(const struct RIAppCore *c, uint32_t inst);
/* Transport against the owned session (null-backend path; the AROS shell
 * routes through the render task when the AHI backend is up). */
void ri_core_play(struct RIAppCore *c);
void ri_core_stop(struct RIAppCore *c);
/* Transport propagation (P-TR2). The panel owns the front-panel transport —
 * its state AND its stop-click counter (ri_tr_stop's clicks law: the first stop
 * holds the cursor, the second goes to the loop start, the third to the song
 * start). The engine owns the session transport. Acting only on a state CHANGE
 * drops every rewind press, because a stop while already stopped changes no
 * state — and then the session cursor stays parked at the song's end, where
 * song_poll_end stops every Play: the song plays once per launch.
 *
 * Call once per round with the panel's (state, clicks) and the last pair acted
 * on; returns RI_TR_SYNC_NONE / _PLAY / _STOP and records the pair it acted on. */
#define RI_TR_SYNC_NONE 0
#define RI_TR_SYNC_PLAY 1
#define RI_TR_SYNC_STOP 2
int ri_core_transport_sync(int panel_state, uint32_t panel_clicks,
    int *last_state, uint32_t *last_clicks);
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

/* au_live_open()'s failure step for "ahi.device would not open at all":
 * audio_ahi_live.c sets err=2 exactly where OpenDevice("ahi.device") fails.
 * On such a machine there is no AHI hardware, so the null backend is the
 * documented offline-render fallback (RI_AUDIO_NULL_MSG) and stays quiet. */
#define RI_AUDIO_ERR_ABSENT 2L
/* 1 when a null-backend fallback must be reported to the user, 0 when it is
 * the documented absent-device case. Every other step (port/alloc/load/task/
 * open-timeout, and err 0 meaning "opened fine, then the run failed") means a
 * device was reachable and the audio path was lost -- on the Dell that is
 * another AHI client already holding the card. Owner 2026-10-02: such a launch
 * used to log one line and carry on, so the window opened, the playlist
 * loaded, the transport played, and there was no sound and no requester --
 * the worst failure mode a music app can have, and strictly worse than the
 * "Cannot load song" requester it was mistaken for. */
int ri_core_audio_failure_is_loud(long err);

/* ---- FX module meters (owner 2026-10-13) -------------------------------
 *
 * The engine has always measured the four FX modules: ri_meter_feed into
 * e->fx_meter[] sits inside both the mono and the stereo insert guards, and
 * struct RILiveMeters has carried float fx_peak[4] into the app on every
 * frame. Nothing read it -- ri_sfx_meter_set had no caller in riapp.c at
 * all -- so the level arrived and was discarded and every FX meter sat dark.
 *
 * The part worth owning HERE is the transpose, not the read. Canvas order
 * is C_FX0..C_FX3 = PCF, DELAY, DIST, COMP. The engine's meter-unit order
 * is DIST=0, PCF=1, DELAY=2, COMP=3. That is a real permutation [1,2,0,3];
 * getting it wrong lights each module's neighbour's meter, which looks
 * entirely plausible on screen. It lives in portable, testable code so the
 * shell cannot transpose it a second way. */
#define RI_CORE_FX_N 4u
/* Canvas order. lvl[i] from ri_core_fx_meters corresponds to this entry. */
extern const uint32_t RI_CORE_FX_ORDER[RI_CORE_FX_N];
/* Engine meter unit for an FX section, or RI_ENGINE_FX_COUNT for a section
 * that has no FX meter -- never a silent fallback to unit 0, which is DIST
 * and would light the wrong box. */
uint32_t ri_core_fx_meter_unit(uint32_t section);
/* Meter snapshot -> the four FX levels in RI_CORE_FX_ORDER, plus the comp
 * gain reduction in dB (<= 0). Returns 1 when a read landed, 0 when
 * busy/skipped, same contract as ri_core_meters. Pure computation; the
 * shell pushes the levels into the panel and repaints each meter box. */
int ri_core_fx_meters(struct RIAppCore *c, int *lvl, float *comp_gr);

/* ONE-LINE SUMMARY OF THE DELAY'S LIVE STATE, for the lane event log.
 *
 * WHY THIS EXISTS. Owner 2026-10-14: "Delay in the Mixer works audibly, in FX
 * it doesn't." Those turned out to be TWO DIFFERENT SESSIONS conflated: the
 * session with 142 F.Back moves had the send at 0, so nothing could be heard
 * to be modulated; the session where the delay was audible never touched the
 * FX knobs. Nothing on screen or in the log showed that, so the only way to
 * tell was to reconstruct two sessions afterwards.
 *
 * The line reports every quantity that decides audibility at once -- send,
 * feedback, steps, return pan, plus live/line-attached flags -- so a future
 * "the delay does nothing" is answerable from the log in one read instead of
 * two ear-proof rounds.
 *
 * Pure formatting over engine state; host-tested. Returns the length written,
 * or <0 on a null argument or a cap too small to hold the line. */
int ri_core_delay_diag(const struct RIAppCore *c, char *buf, uint32_t cap);

#endif
