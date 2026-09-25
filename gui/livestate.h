/* gui/livestate.h — live display state from the audio clock (§12.10 G6a).
 * Pure C, host-tested. Everything here is a one-way projection of a
 * SAMPLE POSITION (the render task's count of frames since playback
 * started) — never a timer beat (review §2.9: the stepproof timer.device
 * beat drifted +347 us per fire). No state is advanced by wall time here.
 *
 * Position law: 16ths elapsed = floor(samples * bpm * 4 / (60 * sr)),
 * integer-exact. Pattern mode: every section loops its own pattern length
 * independently (manual p. 147: "Each one loops independently"); Song
 * mode: the bar display follows 16 sixteenths per bar (4/4).
 * Meters: linear peak -> 0..127 on a dB scale from -36 dBFS (the lowest
 * mark on the Master meter, p. 23) to 0 dBFS (CLIP). Comp level
 * reduction: 0 dB -> 0, 20 dB of reduction -> 127 (E0 full scale: the
 * p. 164 meter carries no numbers besides the centre 0).
 */
#ifndef RI_LIVESTATE_H
#define RI_LIVESTATE_H
#include <stdint.h>

#define RI_LIVE_DB_FLOOR (-36)
#define RI_LIVE_GR_FULL_DB 20

uint64_t ri_live_16ths(uint64_t samples, uint32_t bpm, uint32_t sr); /* 0 on bad args */
/* Step of a section looping `len` steps (1..16, else 16) after n 16ths. */
uint32_t ri_live_step(uint64_t sixteenths, uint32_t len);
/* 0-based bar reached from a 0-based start bar after n 16ths (4/4). */
uint64_t ri_live_bar(uint64_t start_bar, uint64_t sixteenths);
/* Meter level 0..127 from a linear peak (1.0 = 0 dBFS = full, CLIP). */
int ri_live_meter_level(float peak);
/* Level-reduction meter 0..127 from the comp gain change in dB (<= 0). */
int ri_live_gr_level(float gr_db);
#endif
