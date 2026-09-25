/* gui/sectpat.h — Pattern section front-panel behaviour (§12.10 G3/G4).
 * Pure C, host-tested. ReBirth 2.0.1 Owner's Manual p. 147: a section
 * on/off lamp ("essentially the same as an empty Pattern"), Pattern
 * buttons 1-8, Bank buttons A-D, Shuffle on/off and the Steps display
 * (pattern length in 16ths, 1..16, arrows). "No Pattern gets selected
 * until you click one of the Pattern buttons 1 to 8": a Bank click only
 * arms the bank. Each of the 32 patterns keeps its own length ("Different
 * Patterns can have different lengths"). Index = registry index.
 */
#ifndef RI_SECTPAT_H
#define RI_SECTPAT_H
#include <stdint.h>

#define RI_SPAT_OFF 0u       /* Section Off: 1 = section deactivated */
#define RI_SPAT_BANK 1u
#define RI_SPAT_PATTERN 2u
#define RI_SPAT_LENGTH 3u
#define RI_SPAT_SHUFFLE 4u
#define RI_SPAT_NCTL 5u

struct RISectPat {
    uint8_t section;         /* RI_SEC_PAT_* */
    uint8_t off;
    uint8_t bank;            /* selected (playing) pattern = bank * 8 + pattern */
    uint8_t pattern;
    uint8_t armed_bank;      /* Bank button lit; commits with the next Pattern click */
    uint8_t shuffle;
    uint8_t length[32];      /* per pattern, 1..16 */
};

int ri_spat_init(struct RISectPat *s, uint8_t section); /* 0 ok, 2 not a pattern section */
int ri_spat_press(struct RISectPat *s, uint32_t idx);
/* Bank: arm a bank; Pattern: select armed bank + pattern. */
int ri_spat_set_value(struct RISectPat *s, uint32_t idx, int v);
int ri_spat_reset(struct RISectPat *s, uint32_t idx);
int ri_spat_step(struct RISectPat *s, uint32_t idx, int dir); /* Steps arrows */
/* Panel value: Bank = armed bank; Pattern = selected pattern when its bank
 * is the armed one, else -1 (no Pattern button lit); Length = selected
 * pattern's length; switches 0/1. */
int ri_spat_value(const struct RISectPat *s, uint32_t idx);
int ri_spat_led(const struct RISectPat *s, uint32_t idx); /* on/off lamp: 1 = section on */
int ri_spat_selected(const struct RISectPat *s);          /* 0..31 */
#endif
