/* t117_levi_seq_phrase — SEQ phrase-length looper (v2 feature 3c-i).
 * Pure pattern pre-pass: out[i] = in[i % N] over the occurrence
 * length (truncation + wrap-extension); N clamped 1..16, 0/bad
 * fail-closed; off-path is a copy the caller skips (passthrough).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/pattern.h"

int main(void) {
    struct RIPattern p, q;
    uint32_t i;
    ri_pattern_init(&p, RI_PATTERN_KIND_LEVI, 0u);
    ri_pattern_set_length(&p, 16u);
    RI_ASSERT(ri_levi_set(&p, 0u, 0u, 60u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&p, 1u, 0u, 62u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&p, 2u, 0u, 64u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&p, 3u, 0u, 65u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&p, 8u, 0u, 67u, 1) == 0, "set");
    /* Fail-closed. */
    RI_ASSERT(ri_levi_seq_window(0, 4u, &q) == 0u, "null in");
    RI_ASSERT(ri_levi_seq_window(&p, 4u, 0) == 0u, "null out");
    RI_ASSERT(ri_levi_seq_window(&p, 0u, &q) == 0u, "zero len");
    RI_ASSERT(ri_levi_seq_window(&p, 17u, &q) == 0u, "over len");
    {
        struct RIPattern drum;
        ri_pattern_init(&drum, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        RI_ASSERT(ri_levi_seq_window(&drum, 4u, &q) == 0u, "wrong kind");
    }
    /* N=4 over 16: first four steps loop (60 62 64 65); the step-8
     * 67 never sounds (truncated out of the phrase). */
    RI_ASSERT(ri_levi_seq_window(&p, 4u, &q) == 16u, "count");
    for (i = 0u; i < 16u; i++) {
        uint32_t m = i % 4u;
        uint8_t want = m == 3u ? 65u : (uint8_t)(60u + m * 2u);
        RI_ASSERT(ri_levi_get(&q, i, 0u) == want, "step %u note %u", i, ri_levi_get(&q, i, 0u));
        RI_ASSERT(ri_levi_on(&q, i, 0u) == 1, "step %u on", i);
        RI_ASSERT(ri_levi_get(&q, i, 0u) != 67u, "step %u drops 67", i);
    }
    /* N=16: identity. N=1: everything is step 0. */
    RI_ASSERT(ri_levi_seq_window(&p, 16u, &q) == 16u, "full");
    RI_ASSERT(!memcmp(&p.row, &q.row, sizeof p.row), "full identical");
    RI_ASSERT(ri_levi_seq_window(&p, 1u, &q) == 16u, "one");
    for (i = 0u; i < 16u; i++)
        RI_ASSERT(ri_levi_get(&q, i, 0u) == 60u && ri_levi_on(&q, i, 0u) == 1, "one repeats");
    /* Short pattern (len 6), N=4: window covers the occurrence (6). */
    ri_pattern_set_length(&p, 6u);
    RI_ASSERT(ri_levi_seq_window(&p, 4u, &q) == 6u, "short count");
    RI_ASSERT(q.length == 6u, "short len kept");
    RI_ASSERT(ri_levi_get(&q, 4u, 0u) == 60u && ri_levi_get(&q, 5u, 0u) == 62u, "short wrap");
    RI_RESULT("leviseqphrase");
}
