/* memtype_decode.h — pure x86 memory-type decoders for the MEMTYPE probe.
 *
 * Host-compilable, no AROS headers, no Amiga paths: the same files build
 * into tests/unit/t177 and into lane/memtype/memtype.c, so the unit test
 * pins exactly the logic that runs on the Dell.
 *
 * References: Intel SDM Vol. 3A "Memory Cache Control" (MSR numbers, PAT
 * index, effective-type rows — values reproduced from the F1 prompt §0.3;
 * re-check each against the SDM edition before relying on it).
 */
#ifndef MEMTYPE_DECODE_H
#define MEMTYPE_DECODE_H

/* Memory-type encodings (MSR / PAT field values). */
#define MT_UC     0
#define MT_WC     1
#define MT_WT     4
#define MT_WP     5
#define MT_WB     6
#define MT_UCMINUS 7  /* PAT only; never a valid MTRR type */
/* Decoder-level answers (not hardware encodings). */
#define MT_UNKNOWN   (-1) /* combination outside the §0.3 table */
#define MT_OVERLAP_UDEF (-2) /* variable-MTRR overlap the SDM leaves undefined */

/* IA32_PAT raw value -> entry idx (0..7) type, or MT_UNKNOWN for a
 * reserved encoding (2, 3). */
int mt_pat_entry(unsigned long long pat_raw, int idx);

/* PAT index from page-table cache bits.
 * 4 KiB PTE: PAT is bit 7; large page (2M PDE / 1G PDPTE, PS=1): PAT is
 * bit 12. PWT is bit 3, PCD is bit 4 in both cases.
 * index = PAT*4 + PCD*2 + PWT. Returns 0..7, or -1 on bad bit values. */
int mt_pat_index(int pwt, int pcd, int pat);

/* Variable-MTRR PHYSMASK (raw 64-bit MSR) -> covered size in bytes.
 * Derived from the mask with MAXPHYADDR (CPUID.80000008H:EAX[7:0]):
 * size = 1 << (12 + ctz(mask >> 12)), masked to MAXPHYADDR.
 * Returns 0 when no mask bit is set (invalid range). */
unsigned long long mt_mtrr_size(unsigned long long mask_raw, int maxphyaddr);

/* Variable-MTRR PHYSBASE (raw 64-bit MSR) + PHYSMASK -> range base. */
unsigned long long mt_mtrr_base(unsigned long long base_raw,
                                unsigned long long mask_raw);

/* V bit of a PHYSMASK MSR (bit 11). */
int mt_mtrr_valid(unsigned long long mask_raw);

/* Type field of a PHYSBASE MSR (bits 7:0), or -1 when reserved
 * (2, 3, 7 are not valid MTRR types). */
int mt_mtrr_type(unsigned long long base_raw);

/* Effective memory type from the §0.3 table. pat_t and mtrr_t use the
 * MT_* encodings above (pat_t may be MT_UCMINUS). Every row:
 *   PAT UC                 -> UC
 *   PAT UC- + MTRR WC      -> WC
 *   PAT UC- + MTRR other   -> UC
 *   PAT WC                 -> WC
 *   PAT WB  + MTRR UC      -> UC
 *   PAT WB  + MTRR WC      -> WC
 *   PAT WB  + MTRR WB      -> WB
 * Anything else (PAT WT/WP rows, which the §0.3 table does not list)
 * returns MT_UNKNOWN. */
int mt_effective(int pat_t, int mtrr_t);

/* Combine the variable MTRRs matching one address (SDM overlap rule):
 * types[] holds the MTRR types of the matching valid ranges, n entries.
 * UC beats everything; WT+WB (and nothing else) gives WT; a single type
 * wins; any other multi-type overlap is undefined (MT_OVERLAP_UDEF).
 * n == 0 means no range matched (caller falls back to the default type). */
int mt_mtrr_overlap(const int *types, int n);

/* Human-readable name for an MT_* type ("UC","WC","WT","WP","WB","UC-",
 * "??" for unknown). Never returns NULL. */
const char *mt_name(int t);

#endif
