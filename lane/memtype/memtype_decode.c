/* memtype_decode.c — see memtype_decode.h. Pure C, no platform headers. */

#include "memtype_decode.h"

int mt_pat_entry(unsigned long long pat_raw, int idx)
{
    unsigned int b;
    if (idx < 0 || idx > 7)
        return MT_UNKNOWN;
    b = (unsigned int)((pat_raw >> (idx * 8)) & 7u);
    switch (b) {
    case MT_UC:
    case MT_WC:
    case MT_WT:
    case MT_WP:
    case MT_WB:
    case MT_UCMINUS:
        return (int)b;
    default:
        return MT_UNKNOWN;
    }
}

int mt_pat_index(int pwt, int pcd, int pat)
{
    if ((pwt & ~1) || (pcd & ~1) || (pat & ~1))
        return -1;
    /* PAT index = PAT*4 + PCD*2 + PWT (SDM Vol. 3A, Memory Cache Control). */
    return pat * 4 + pcd * 2 + pwt;
}

unsigned long long mt_mtrr_size(unsigned long long mask_raw, int maxphyaddr)
{
    unsigned long long m;
    int shift;
    if (maxphyaddr < 12 || maxphyaddr > 64)
        return 0;
    m = mask_raw >> 12;
    /* Keep only the implemented physical-address bits. */
    if (maxphyaddr < 64)
        m &= (~0ULL >> (64 - (maxphyaddr - 12)));
    if (m == 0)
        return 0;
    shift = 0;
    while ((m & 1ULL) == 0) {
        m >>= 1;
        shift++;
    }
    return 1ULL << (12 + shift);
}

unsigned long long mt_mtrr_base(unsigned long long base_raw,
                                unsigned long long mask_raw)
{
    return base_raw & mask_raw & (~0ULL << 12);
}

int mt_mtrr_valid(unsigned long long mask_raw)
{
    return (mask_raw & (1ULL << 11)) ? 1 : 0;
}

int mt_mtrr_type(unsigned long long base_raw)
{
    switch (base_raw & 7u) {
    case MT_UC:
    case MT_WC:
    case MT_WT:
    case MT_WP:
    case MT_WB:
        return (int)(base_raw & 7u);
    default:
        return -1;
    }
}

int mt_effective(int pat_t, int mtrr_t)
{
    /* Effective page-level memory type, prompt §0.3 table rows. */
    if (pat_t == MT_UC)
        return MT_UC;                 /* UC + any -> UC */
    if (pat_t == MT_UCMINUS)
        return (mtrr_t == MT_WC) ? MT_WC : MT_UC; /* UC- + WC -> WC, else UC */
    if (pat_t == MT_WC)
        return MT_WC;                 /* WC + any -> WC */
    if (pat_t == MT_WB) {
        if (mtrr_t == MT_UC)
            return MT_UC;             /* WB + UC -> UC */
        if (mtrr_t == MT_WC)
            return MT_WC;             /* WB + WC -> WC */
        if (mtrr_t == MT_WB)
            return MT_WB;             /* WB + WB -> WB */
    }
    return MT_UNKNOWN;
}

int mt_mtrr_overlap(const int *types, int n)
{
    int i, distinct = 0, seen[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    if (n <= 0 || !types)
        return MT_UNKNOWN;
    for (i = 0; i < n; i++) {
        if (types[i] < 0 || types[i] > MT_UCMINUS)
            return MT_OVERLAP_UDEF;
        if (!seen[types[i]]) {
            seen[types[i]] = 1;
            distinct++;
        }
    }
    if (seen[MT_UC])
        return MT_UC;                 /* UC wins over everything */
    if (distinct == 1)
        return types[0];
    if (distinct == 2 && seen[MT_WT] && seen[MT_WB])
        return MT_WT;                 /* WT combined with WB gives WT */
    return MT_OVERLAP_UDEF;           /* all other overlaps undefined */
}

const char *mt_name(int t)
{
    switch (t) {
    case MT_UC:
        return "UC";
    case MT_WC:
        return "WC";
    case MT_WT:
        return "WT";
    case MT_WP:
        return "WP";
    case MT_WB:
        return "WB";
    case MT_UCMINUS:
        return "UC-";
    default:
        return "??";
    }
}
