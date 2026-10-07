/* t177_memtype_decode — MEMTYPE decoder laws (framebuffer memtype F1).
 *
 * Pins the pure decoders in lane/memtype/memtype_decode.c that the Dell
 * MEMTYPE probe links in: PAT entry/index, MTRR mask->size/base, the full
 * §0.3 effective-type table, and the variable-MTRR overlap rule.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "lane/memtype/memtype_decode.h"
#include "lane/memtype/memtype_decode.c" /* same TU the Dell probe links */

int main(void)
{
    /* Power-on PAT: PA0=WB PA1=WT PA2=UC- PA3=UC PA4=WB PA5=WT PA6=UC- PA7=UC. */
    unsigned long long pat = 0x0007040600070406ULL;
    int t[3];

    RI_ASSERT(mt_pat_entry((unsigned long long)pat, 0) == MT_WB, "PA0 WB");
    RI_ASSERT(mt_pat_entry((unsigned long long)pat, 1) == MT_WT, "PA1 WT");
    RI_ASSERT(mt_pat_entry((unsigned long long)pat, 2) == MT_UCMINUS, "PA2 UC-");
    RI_ASSERT(mt_pat_entry((unsigned long long)pat, 3) == MT_UC, "PA3 UC");
    RI_ASSERT(mt_pat_entry((unsigned long long)pat, 4) == MT_WB, "PA4 WB");
    RI_ASSERT(mt_pat_entry((unsigned long long)pat, 5) == MT_WT, "PA5 WT");
    RI_ASSERT(mt_pat_entry((unsigned long long)pat, 6) == MT_UCMINUS, "PA6 UC-");
    RI_ASSERT(mt_pat_entry((unsigned long long)pat, 7) == MT_UC, "PA7 UC");
    RI_ASSERT(mt_pat_entry(0x0000000000000002ULL, 0) == MT_UNKNOWN, "reserved PAT enc 2");
    RI_ASSERT(mt_pat_entry(0x0000000000000003ULL, 0) == MT_UNKNOWN, "reserved PAT enc 3");
    RI_ASSERT(mt_pat_entry((unsigned long long)pat, 8) == MT_UNKNOWN, "PAT idx range");

    /* PAT index, both bit positions of the PAT bit. */
    RI_ASSERT(mt_pat_index(0, 0, 0) == 0, "idx 000");
    RI_ASSERT(mt_pat_index(1, 0, 0) == 1, "idx PWT");
    RI_ASSERT(mt_pat_index(0, 1, 0) == 2, "idx PCD");
    RI_ASSERT(mt_pat_index(1, 1, 0) == 3, "idx PWT+PCD");
    RI_ASSERT(mt_pat_index(0, 0, 1) == 4, "idx PAT");
    RI_ASSERT(mt_pat_index(1, 1, 1) == 7, "idx all");
    RI_ASSERT(mt_pat_index(2, 0, 0) == -1, "idx bad bit");

    /* MTRR mask->size: BIOS-style 256 MB range at 0xE0000000, MAXPHYADDR 36.
     * PHYSMASK = 0x0000000FF0000800 (V=1, mask 0xFF0000000). */
    RI_ASSERT(mt_mtrr_valid(0x0000000FF0000800ULL) == 1, "V bit set");
    RI_ASSERT(mt_mtrr_valid(0x0000000FF0000000ULL) == 0, "V bit clear");
    RI_ASSERT(mt_mtrr_size(0x0000000FF0000800ULL, 36) == 0x10000000ULL, "256M size");
    RI_ASSERT(mt_mtrr_base(0x0000000E0000006ULL, 0x0000000FF0000800ULL) == 0xE0000000ULL,
              "E000 base");
    RI_ASSERT(mt_mtrr_type(0x0000000E0000006ULL) == MT_WB, "base type WB");
    RI_ASSERT(mt_mtrr_type(0x0000000E0000001ULL) == MT_WC, "base type WC");
    RI_ASSERT(mt_mtrr_type(0x0000000E0000002ULL) == -1, "base type reserved");
    /* 4 KiB minimum range: all implemented mask bits set (MAXPHYADDR 36). */
    RI_ASSERT(mt_mtrr_size(0x000000FFFFFFF800ULL, 36) == 0x1000ULL, "4K size");
    RI_ASSERT(mt_mtrr_size(0ULL, 36) == 0ULL, "empty mask");

    /* Effective-type table, every §0.3 row. */
    RI_ASSERT(mt_effective(MT_UC, MT_UC) == MT_UC, "UC+UC");
    RI_ASSERT(mt_effective(MT_UC, MT_WB) == MT_UC, "UC+WB");
    RI_ASSERT(mt_effective(MT_UC, MT_WC) == MT_UC, "UC+WC");
    RI_ASSERT(mt_effective(MT_UCMINUS, MT_WC) == MT_WC, "UC-+WC");
    RI_ASSERT(mt_effective(MT_UCMINUS, MT_WB) == MT_UC, "UC-+WB");
    RI_ASSERT(mt_effective(MT_UCMINUS, MT_UC) == MT_UC, "UC-+UC");
    RI_ASSERT(mt_effective(MT_WC, MT_UC) == MT_WC, "WC+UC");
    RI_ASSERT(mt_effective(MT_WC, MT_WB) == MT_WC, "WC+WB");
    RI_ASSERT(mt_effective(MT_WB, MT_UC) == MT_UC, "WB+UC");
    RI_ASSERT(mt_effective(MT_WB, MT_WC) == MT_WC, "WB+WC");
    RI_ASSERT(mt_effective(MT_WB, MT_WB) == MT_WB, "WB+WB");
    RI_ASSERT(mt_effective(MT_WT, MT_WB) == MT_UNKNOWN, "WT row not in table");

    /* Variable-MTRR overlap rule. */
    t[0] = MT_WB;
    RI_ASSERT(mt_mtrr_overlap(t, 1) == MT_WB, "single");
    t[0] = MT_WB; t[1] = MT_UC;
    RI_ASSERT(mt_mtrr_overlap(t, 2) == MT_UC, "UC wins");
    t[0] = MT_WT; t[1] = MT_WB;
    RI_ASSERT(mt_mtrr_overlap(t, 2) == MT_WT, "WT+WB=WT");
    t[0] = MT_WB; t[1] = MT_WC;
    RI_ASSERT(mt_mtrr_overlap(t, 2) == MT_OVERLAP_UDEF, "WB+WC undefined");
    t[0] = MT_WT; t[1] = MT_WB; t[2] = MT_WC;
    RI_ASSERT(mt_mtrr_overlap(t, 3) == MT_OVERLAP_UDEF, "triple undefined");
    RI_ASSERT(mt_mtrr_overlap(t, 0) == MT_UNKNOWN, "no match");

    RI_ASSERT(mt_name(MT_UC)[0] == 'U', "name UC");
    RI_ASSERT(mt_name(MT_WC)[0] == 'W', "name WC");
    RI_ASSERT(mt_name(MT_UCMINUS)[2] == '-', "name UC-");

    RI_RESULT("t177_memtype_decode");
}
