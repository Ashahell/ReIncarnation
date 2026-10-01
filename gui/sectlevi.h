/* gui/sectlevi.h — Levi section front-panel behaviour (owner 2026-09-28).
 * Pure C, host-tested. 909-style lane select (SELECTOR) + 16 step
 * buttons (toggle the selected lane, middle C on enable) with 303-style
 * edit step + piano keyboard for pitch (13 chromatic keys C..C+).
 * Values index = registry index within the section (reg_id & 0xFF).
 */
#ifndef RI_SECTLEVI_H
#define RI_SECTLEVI_H
#include <stdint.h>
#include "engine/seq/pattern.h"
#include "engine/dsp/levi.h"

#define RI_SLEVI_NCTL 221u
#define RI_SLEVI_CUTOFF 0u      /* digital filter cutoff */
#define RI_SLEVI_RESO 1u        /* digital filter resonance */
#define RI_SLEVI_RATIO 3u       /* modulator ratio */
#define RI_SLEVI_SELECT 4u      /* Lane Selection: 0..5 */
#define RI_SLEVI_MODE 2u          /* FM/PM toggle */
#define RI_SLEVI_ALGO 37u         /* Algorithm select 0..63 (P3: 64 presets) */
#define RI_SLEVI_ALGOB 38u        /* v1 morph target 0..63 (legacy row) */
#define RI_SLEVI_MORPH 39u        /* Morph position 0..100 */
#define RI_SLEVI_OPSEL 40u        /* Op selection 0..7 (UI-only) */
#define RI_SLEVI_OPMODE 41u       /* Op Mode 0..6 (packed op*16+mode) */
#define RI_SLEVI_FTYPE 42u        /* Filter type 0..3 */
#define RI_SLEVI_DRIVE 43u        /* Drive 0..127 */
#define RI_SLEVI_CUTOFF2 44u      /* Analog cutoff */
#define RI_SLEVI_RESO2 45u        /* Analog reso */
#define RI_SLEVI_ATTACK 46u       /* Attack time */
#define RI_SLEVI_DECAY 47u        /* Decay time */
#define RI_SLEVI_SUSTAIN 48u      /* Sustain level */
#define RI_SLEVI_RELEASE 49u      /* Release time */
#define RI_SLEVI_LOOP 50u /* Envelope loop */
#define RI_SLEVI_ALGODISP 51u   /* Central algorithm readout 1..64, 0 = custom */
#define RI_SLEVI_ARPON 52u        /* Arp on (UI-only, binds later) */
#define RI_SLEVI_ARPRATE 53u      /* Arp rate (UI-only) */
#define RI_SLEVI_SEQON 54u        /* Seq on (UI-only) */
#define RI_SLEVI_SEQLEN 55u       /* Seq length 1..16 (UI-only) */
#define RI_SLEVI_ROUTE0 56u       /* Matrix routes 0..7 (UI-only) */
#define RI_SLEVI_FXPRE 64u        /* PreFX (UI-only) */
#define RI_SLEVI_FXDLY 65u        /* Delay (UI-only) */
#define RI_SLEVI_FXREV 66u        /* Reverb (UI-only) */
#define RI_SLEVI_FXPOST 67u       /* PostFX (UI-only) */
/* Hardware page UI (fidelity plan P1, owner 2026-09-30): MODULE SELECT
 * keys pick a page, the 8 MASTER CONTROL encoders edit that page's
 * slots, the display shows the page. Encoders are UI controls: the
 * app sends the TARGET parameter (ri_slevi_ctl_idx), so automation
 * keys and the control plane are unchanged. */
#define RI_SLEVI_MODULE 68u       /* selected module/page 0..RI_SLEVI_NMOD-1 */
#define RI_SLEVI_ENC0 69u         /* 8 encoders: 69..76 */
#define RI_SLEVI_NENC 8u
#define RI_SLEVI_PAGE 77u         /* display (value = module) */
/* Osc Env Level & Bias knobs (fidelity P2, manual p. 54): offsets over
 * every oscillator envelope, 64 = none. Keys 0x0E27..0x0E2A. */
#define RI_SLEVI_BIAS_ENVL 78u
#define RI_SLEVI_BIAS_ATK 79u
#define RI_SLEVI_BIAS_DEC 80u
#define RI_SLEVI_BIAS_REL 81u
#define RI_SLEVI_PAGEUP 82u       /* PAGE up / down (page within module) */
#define RI_SLEVI_PAGEDN 83u
/* Algorithm modes (fidelity P3, manual pp. 58-61): Single / Morph /
 * Custom, the 8-slot morph list, morph position, solo and mute. Keys
 * 0x0E2B..0x0E37; the custom grid rows are per-oscillator TGT1..3. */
#define RI_SLEVI_AMODE 84u
#define RI_SLEVI_SLOT0 85u        /* 8 slots: 85..92 (0..63 algo, 64 silence, 65 off) */
#define RI_SLEVI_MPOS 93u         /* morph position 0..127 across the live slots */
#define RI_SLEVI_SOLO 94u         /* 0 off, n = osc n */
#define RI_SLEVI_MUTELO 95u       /* osc 1-7 mute bits */
#define RI_SLEVI_MUTEHI 96u       /* osc 8 mute */
/* Filters + VCA (fidelity P4, pp. 62-70), keys 0x0E38..0x0E44. FTYPE
 * (the v1 4-way type) becomes a legacy row; DRIVE is the analog
 * pre-drive. */
#define RI_SLEVI_DTYPE 97u        /* digital model 0..17 */
#define RI_SLEVI_DMORPH 98u       /* morph (SVF, vowel) / drive (others) */
#define RI_SLEVI_DPOST 99u        /* drive position: 0 pre, 1 post */
#define RI_SLEVI_VORDER 100u      /* vowel order 0..7 */
#define RI_SLEVI_DKEYTRK 101u
#define RI_SLEVI_DLFO1 102u
#define RI_SLEVI_DLEVEL 103u
#define RI_SLEVI_AKEYTRK 104u
#define RI_SLEVI_ALFO2 105u
#define RI_SLEVI_OSCLVL 106u
#define RI_SLEVI_VCALVL 107u
#define RI_SLEVI_PATCHLVL 108u
#define RI_SLEVI_VLFO3 109u
/* Pre-wired envelope amounts and VCA initial level (fidelity P5), keys
 * 0x0E45..0x0E47; the ENV 1 / ENV 2 top-panel knobs bind here. */
#define RI_SLEVI_DENV1 110u
#define RI_SLEVI_AENV2 111u
#define RI_SLEVI_VINIT 112u
/* Macros (fidelity P5b): knobs 113..120, buttons 121..128. */
#define RI_SLEVI_MKNOB0 113u
#define RI_SLEVI_MBTN0 121u
/* Voice allocator (fidelity P6a, manual pp. 87-96): 129..131. */
#define RI_SLEVI_POLYMODE 129u
#define RI_SLEVI_UDENSITY 130u
#define RI_SLEVI_ULIMIT 131u
/* Voice params (fidelity P6b): 132..144. */
#define RI_SLEVI_VDETUNE 132u
#define RI_SLEVI_VAFEEL 133u
#define RI_SLEVI_VRNDPH 134u
#define RI_SLEVI_VPAN 135u
#define RI_SLEVI_VWIDTH 136u
#define RI_SLEVI_VPANMODE 137u
#define RI_SLEVI_VBENDRNG 138u
#define RI_SLEVI_VVIBRATE 139u
#define RI_SLEVI_VVIBAMT 140u
#define RI_SLEVI_VVIBDLY 141u
#define RI_SLEVI_VGLIDE 142u
#define RI_SLEVI_VGLTIME 143u
#define RI_SLEVI_VGLCURVE 144u
/* Stereo + scales (fidelity P6c): 145..158. */
#define RI_SLEVI_VINTAGE 145u
#define RI_SLEVI_VSCALE 146u
#define RI_SLEVI_VMICRO 147u
#define RI_SLEVI_VKEYLOCK 148u
#define RI_SLEVI_VSPREAD 149u
#define RI_SLEVI_VOSCPAN1 150u   /* 8 osc pans: 150..157 */
/* Delay (fidelity P7a, manual pp. 83-86): 158..164. */
#define RI_SLEVI_DLYTYPE 158u
#define RI_SLEVI_DLYTIME 159u
#define RI_SLEVI_DLYFB 160u
#define RI_SLEVI_DLYWTONE 161u
#define RI_SLEVI_DLYFBTONE 162u
#define RI_SLEVI_DLYDRYWET 163u
#define RI_SLEVI_DLYBPM 164u
/* Reverb (fidelity P7b): 165..171. */
#define RI_SLEVI_RTYPE 165u
#define RI_SLEVI_RPREDLY 166u
#define RI_SLEVI_RTIME 167u
#define RI_SLEVI_RTONE 168u
#define RI_SLEVI_RHIDAMP 169u
#define RI_SLEVI_RLODAMP 170u
#define RI_SLEVI_RDRYWET 171u
#define RI_SLEVI_RFREEZE 172u
/* Mod FX pre/post (fidelity P7c): 173..182. */
#define RI_SLEVI_PTYPE 173u
#define RI_SLEVI_PPRESET 174u
#define RI_SLEVI_PP1 175u
#define RI_SLEVI_PP2 176u
#define RI_SLEVI_PDRYWET 177u
#define RI_SLEVI_OTYPE 178u
#define RI_SLEVI_OPRESET 179u
#define RI_SLEVI_OP1 180u
#define RI_SLEVI_OP2 181u
#define RI_SLEVI_ODRYWET 182u
/* Device arp (fidelity P8b): 183..195. */
#define RI_SLEVI_ARPOCTMODE 183u
#define RI_SLEVI_ARPOCTRANGE 184u
#define RI_SLEVI_ARPGATE 185u
#define RI_SLEVI_ARPMODE 186u
#define RI_SLEVI_ARPLEN 187u
#define RI_SLEVI_ARPPHRASE 188u
#define RI_SLEVI_ARPENTROPY 189u
#define RI_SLEVI_ARPSWING 190u
#define RI_SLEVI_ARPRATCHET 191u
#define RI_SLEVI_ARPCHANCE 192u
#define RI_SLEVI_ARPLATCH 193u
#define RI_SLEVI_ARPCLOCK 194u
#define RI_SLEVI_ARPSTEPPOFF 195u
/* Device sequencer (fidelity P8c): 196..210. */
#define RI_SLEVI_SEQRATE 196u
#define RI_SLEVI_SEQMODE 197u
#define RI_SLEVI_SEQSWING 198u
#define RI_SLEVI_SEQGATE 199u
#define RI_SLEVI_SEQPROB 200u
#define RI_SLEVI_SEQDRIFT 201u
#define RI_SLEVI_SEQTRANSP 202u
#define RI_SLEVI_SEQTRKLEN 203u
#define RI_SLEVI_SEQREC 204u
#define RI_SLEVI_SEQSTEP 205u
#define RI_SLEVI_SEQCLEAR 206u
#define RI_SLEVI_SEQSTRIG 207u
#define RI_SLEVI_SEQSPROB 208u
#define RI_SLEVI_SEQSDRIFT 209u
#define RI_SLEVI_SEQSENTR 210u
/* Ribbon (fidelity P8d): 211..213. */
#define RI_SLEVI_RBNMODE 211u
#define RI_SLEVI_RBNPOS 212u
#define RI_SLEVI_RBNTOUCH 213u
/* LFO step editor (fidelity P8e): 214 is the panel-only page gate (the
 * editor slots carry RI_LEVI_LSKEYs, not registry rows). */
#define RI_SLEVI_LFEDIT 214u
/* Performance amounts (fidelity P9b): 215..220, the dim VEL>ENV / POLYAT
 * slots P5 already drew on the two filter pages and the VCA page. */
#define RI_SLEVI_DVEL 215u
#define RI_SLEVI_DPAT 216u
#define RI_SLEVI_AVEL 217u
#define RI_SLEVI_APAT 218u
#define RI_SLEVI_VVEL 219u
#define RI_SLEVI_VPAT 220u
/* Modules (page ids): OSC n (with opsel), the Oscillator Group Edit
 * keys, the MODULE SELECT chain, and the Algo/Arp/Seq/Matrix/Voice
 * pages behind their own buttons. */
#define RI_SLEVI_M_OSC 0u
#define RI_SLEVI_M_GMODE 1u
#define RI_SLEVI_M_GWAVE 2u
#define RI_SLEVI_M_GPITCH 3u
#define RI_SLEVI_M_GFINE 4u
#define RI_SLEVI_M_GFEEDBK 5u
#define RI_SLEVI_M_GLEVEL 6u
#define RI_SLEVI_M_GDELAY 7u
#define RI_SLEVI_M_GATTACK 8u
#define RI_SLEVI_M_GHOLD 9u
#define RI_SLEVI_M_GDECAY 10u
#define RI_SLEVI_M_GSUSTAIN 11u
#define RI_SLEVI_M_GRELEASE 12u
#define RI_SLEVI_M_ENV1 13u       /* ENV 1..5: 13..17 */
#define RI_SLEVI_M_DFILT 18u
#define RI_SLEVI_M_AFILT 19u
#define RI_SLEVI_M_VCA 20u
#define RI_SLEVI_M_PREFX 21u
#define RI_SLEVI_M_DELAY 22u
#define RI_SLEVI_M_REVERB 23u
#define RI_SLEVI_M_POSTFX 24u
#define RI_SLEVI_M_LFO1 25u       /* LFO 1..5: 25..29 */
#define RI_SLEVI_M_ALGO 30u
#define RI_SLEVI_M_ARP 31u
#define RI_SLEVI_M_SEQ 32u
#define RI_SLEVI_M_MATRIX 33u
#define RI_SLEVI_M_VOICE 34u
#define RI_SLEVI_M_MACRO 35u      /* MACRO ASSIGN (P5b) */
#define RI_SLEVI_M_RIBBON 36u    /* ribbon (P8d) */
#define RI_SLEVI_NMOD 37u
#define RI_SLEVI_STEP 5u     /* Step: edit_step + 1 (wraps) */
#define RI_SLEVI_BACK 6u     /* Back: edit_step - 1 (wraps) */
#define RI_SLEVI_DISPLAY 7u     /* EDIT STEP readout */
#define RI_SLEVI_STEP0 8u       /* 16 step buttons: 8..23 */
#define RI_SLEVI_KEY0 24u       /* 13 pitch keys: 24..36 = C..C+ */
#define RI_SLEVI_KEYS 13u
#define RI_SLEVI_MIDDLE_C 60u

struct RISectLevi {
    uint8_t section;           /* RI_SEC_LEVI */
    uint8_t sel;               /* selected lane 0..5 */
    uint8_t edit_step;         /* 0..15 */
    uint8_t opsel;             /* selected operator 0..7 */
    int16_t val[RI_SLEVI_NCTL];
    uint8_t opmode[RI_LEVI_NOPS];      /* panel truth per op (engine follows) */
    uint8_t page;                      /* page within the module (0-based) */
    uint8_t pad[3];
    uint8_t opv[RI_LEVI_NOPS][RI_LEVI_OP_NPARAM]; /* per-op UI values (P2) */
    uint8_t mev[RI_LEVI_NMENV][RI_LEVI_OP_NPARAM];   /* ENV 1-5 UI values (P5) */
    uint8_t lfv[RI_LEVI_NLFO][16];                   /* LFO 1-5 UI values (P5) */
    uint8_t opbpm[RI_LEVI_NOPS]; /* per-op ENV BPM flags (P8a panel truth) */
    uint8_t mebpm[RI_LEVI_NMENV]; /* per-menv BPM flags (P8a panel truth) */
    uint8_t bpmpad[3];
    uint8_t mxv[RI_LEVI_MX_NSLOTS][4];               /* matrix routes: source, module, param, depth (P5b) */
    uint8_t mrv[RI_LEVI_NMACRO][RI_LEVI_MACRO_NR][4]; /* macro routes: module, param, depth, button value */
    uint8_t lfsc[RI_LEVI_NLFO]; /* LFO step cursor 0..63 (P8e panel truth) */
    uint8_t lfsv[RI_LEVI_NLFO]; /* value at the cursor (last written) */
    uint8_t lfsr[RI_LEVI_NLFO]; /* ramp gate */
    struct RIPattern pat;      /* chord kind, class Levi */
};

int ri_slevi_init(struct RISectLevi *s);
int ri_slevi_press(struct RISectLevi *s, uint32_t idx);
int ri_slevi_set_value(struct RISectLevi *s, uint32_t idx, int v);
int ri_slevi_reset(struct RISectLevi *s, uint32_t idx);
/* Step LED: selected lane sounding; key LED: pitch match at
 * (edit step, selected lane). */
int ri_slevi_led(const struct RISectLevi *s, uint32_t idx);
/* EDIT STEP 1..16. */
int ri_slevi_display(const struct RISectLevi *s);
/* Central algorithm readout 1..64 (panel truth); 0 in custom mode or on NULL. */
int ri_slevi_algo_display(const struct RISectLevi *s);

/* Page UI (P1). Value of any control as drawn (encoders read their
 * target, scaled to 0..127). */
int ri_slevi_value(const struct RISectLevi *s, uint32_t idx);
/* Control index the app sends for a hit: an encoder's live target,
 * else idx itself. */
uint32_t ri_slevi_ctl_idx(const struct RISectLevi *s, uint32_t idx);
/* Explicit key for a hit: 1 (key and val set) when the encoder edits an
 * oscillator param (P2: 0x0F00 | op << 5 | param) or an ENV / LFO param
 * (P5: block 0x10), val in the param's range; else 0 (send
 * ri_slevi_ctl_idx as usual). */
int ri_slevi_ctl_key(const struct RISectLevi *s, uint32_t idx, uint16_t *key, int *val);
/* Pages in the current module (1.. ; OSC has 5) and the current page. */
uint32_t ri_slevi_page_count(const struct RISectLevi *s);
/* Encoder k (0..7) on the current page: 1 when it edits a live
 * parameter, 0 for a slot whose engine lands in a later phase. */
int ri_slevi_enc_live(const struct RISectLevi *s, uint32_t k);
/* 1 when some module page slot edits idx (the control needs no panel
 * item of its own: the encoders reach it). Pure, table-only. */
int ri_slevi_page_reaches(uint32_t idx);
/* 1 for the v1 section-wide rows superseded by the per-oscillator
 * params (fidelity P2): global ratio, packed op mode, all-op envelope,
 * the v1 two-algorithm morph superseded by the slot list (P3), and the
 * v1 4-way filter type superseded by the 18 models (P4).
 * They stay registered and automatable (old songs) with no panel item. */
int ri_slevi_legacy(uint32_t idx);
/* Page title and slot name (static strings, never NULL). */
const char *ri_slevi_page_title(const struct RISectLevi *s);
const char *ri_slevi_enc_name(const struct RISectLevi *s, uint32_t k);
/* Slot value text into buf (always NUL-terminated; "" for dead slots). */
void ri_slevi_enc_text(const struct RISectLevi *s, uint32_t k, char *buf, uint32_t cap);

#endif
