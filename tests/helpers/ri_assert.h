#ifndef RI_ASSERT_H
#define RI_ASSERT_H
#include <stdio.h>
static int ri_fail_count = 0;
#define RI_ASSERT(cond, ...) do { \
    if (!(cond)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); ri_fail_count++; } \
} while (0)
#define RI_RESULT(name) do { printf(ri_fail_count ? "FAIL %d\n" : "PASS " name "\n", ri_fail_count); return ri_fail_count != 0; } while (0)
#endif
