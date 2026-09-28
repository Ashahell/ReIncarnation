/* t101_levi_pattern — Levi chord-step pattern kind (owner 2026-09-28).
 * Polyphonic v1 decision: per-step note lanes (drum shape, melodic
 * content). Init/length/set/get round-trip, fail-closed geometry,
 * bank integration, all-REST... all-silent default.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/pattern.h"

int main(void) {
    struct RIPattern p;
    struct RIPatternBank b;
    uint32_t s;
    ri_pattern_init(&p, RI_PATTERN_KIND_LEVI, 0u);
    RI_ASSERT(p.kind == RI_PATTERN_KIND_LEVI, "kind");
    RI_ASSERT(p.length == RI_PATTERN_STEPS, "len");
    RI_ASSERT(ri_pattern_valid(&p) == 0, "valid init");
    for (s = 0u; s < 16u; s++)
        RI_ASSERT(ri_levi_on(&p, s, 0u) == 0, "silent %u", s);
    RI_ASSERT(ri_levi_set(&p, 0u, 0u, 60u, 1) == 0, "set rc");
    RI_ASSERT(ri_levi_set(&p, 0u, 1u, 64u, 1) == 0, "set rc2");
    RI_ASSERT(ri_levi_set(&p, 0u, 5u, 67u, 1) == 0, "set rc3");
    RI_ASSERT(ri_levi_on(&p, 0u, 0u) == 1, "on");
    RI_ASSERT(ri_levi_get(&p, 0u, 0u) == 60u, "note");
    RI_ASSERT(ri_levi_get(&p, 0u, 1u) == 64u, "note2");
    RI_ASSERT(ri_levi_on(&p, 1u, 0u) == 0, "other step silent");
    RI_ASSERT(ri_levi_set(&p, 0u, 0u, 60u, 0) == 0, "clear rc");
    RI_ASSERT(ri_levi_on(&p, 0u, 0u) == 0, "cleared");
    RI_ASSERT(ri_levi_set(&p, 16u, 0u, 60u, 1) == 2, "bad step");
    RI_ASSERT(ri_levi_set(&p, 0u, 6u, 60u, 1) == 2, "bad lane");
    RI_ASSERT(ri_levi_set(&p, 0u, 0u, 128u, 1) == 2, "bad note");
    RI_ASSERT(ri_levi_set(0, 0u, 0u, 60u, 1) == 2, "null set");
    RI_ASSERT(ri_levi_get(&p, 0u, 6u) == 0xFFu, "bad lane get");
    RI_ASSERT(ri_levi_get(0, 0u, 0u) == 0xFFu, "null get");
    RI_ASSERT(ri_levi_on(0, 0u, 0u) == 0, "null on");
    /* A 303 pattern refuses Levi writes and vice versa. */
    ri_pattern_init(&p, RI_PATTERN_KIND_303, 0u);
    RI_ASSERT(ri_levi_set(&p, 0u, 0u, 60u, 1) == 2, "wrong kind");
    RI_ASSERT(ri_p303_set(&p, 0u, 6u, 0u) == 0, "303 ok");
    /* Bank integration. */
    ri_bank_init(&b, 4u, RI_PATTERN_KIND_LEVI, 0u);
    RI_ASSERT(b.kind == RI_PATTERN_KIND_LEVI, "bank kind");
    RI_ASSERT(b.instance == 4u, "bank inst");
    RI_ASSERT(ri_pattern_valid(&b.pat[0]) == 0, "bank slot valid");
    ri_bank_init(0, 0u, RI_PATTERN_KIND_LEVI, 0u); /* null: no-op, no crash */
    RI_RESULT("levipattern");
}
