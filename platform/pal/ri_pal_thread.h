/* ri_pal_thread.h — PAL atomics (portability plan T1, §3.1).
 * Header-only, C99, includes only <stdint.h>/<stddef.h> (plan §2 gate).
 * No <stdatomic.h>: GCC/Clang lower to __atomic acquire/release builtins
 * (no include needed); MSVC uses a compiler barrier (x86-64 TSO; ARM64
 * Windows needs a real fence — T11 owns that switch).
 * Usage: cross-thread words (SPSC head/tail, publish front/staged,
 * snapshot pointers, live request/state words, meter seqlock). Plain
 * same-side counters stay plain uint32_t. Single-writer increments go
 * through load_acq + store_rel (or ri_atomic_fetch_add_rel below).
 * Thread/event/time arrive with T4/T8; this header is atomics only.
 */
#ifndef RI_PAL_THREAD_H
#define RI_PAL_THREAD_H
#include <stdint.h>
#include <stddef.h>

typedef struct { volatile uint32_t v; } ri_atomic_u32;
typedef struct { void *volatile v; } ri_atomic_ptr;

#if defined(_MSC_VER) && !defined(__clang__)
void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)
#endif

static inline uint32_t ri_atomic_load_acq(const ri_atomic_u32 *a) {
#if defined(__GNUC__) || defined(__clang__)
    uint32_t out;
    __atomic_load((uint32_t *)&a->v, &out, __ATOMIC_ACQUIRE);
    return out;
#elif defined(_MSC_VER)
    uint32_t out = a->v;
    _ReadWriteBarrier();
    return out;
#else
    return a->v;
#endif
}

static inline void ri_atomic_store_rel(ri_atomic_u32 *a, uint32_t v) {
#if defined(__GNUC__) || defined(__clang__)
    __atomic_store((uint32_t *)&a->v, &v, __ATOMIC_RELEASE);
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
    a->v = v;
#else
    a->v = v;
#endif
}

/* Single-writer RMW add (stats, seqlock tickets). Full barrier pair on
 * GCC/Clang; compiler-barrier + volatile op on MSVC (TSO-correct). */
static inline void ri_atomic_fetch_add_rel(ri_atomic_u32 *a, uint32_t d) {
#if defined(__GNUC__) || defined(__clang__)
    __atomic_fetch_add((uint32_t *)&a->v, d, __ATOMIC_ACQ_REL);
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
    a->v = (uint32_t)(a->v + d);
    _ReadWriteBarrier();
#else
    a->v = (uint32_t)(a->v + d);
#endif
}

static inline void *ri_atomic_ptr_load_acq(const ri_atomic_ptr *a) {
#if defined(__GNUC__) || defined(__clang__)
    void *out;
    __atomic_load((void **)&a->v, &out, __ATOMIC_ACQUIRE);
    return out;
#elif defined(_MSC_VER)
    void *out = a->v;
    _ReadWriteBarrier();
    return out;
#else
    return a->v;
#endif
}

static inline void ri_atomic_ptr_store_rel(ri_atomic_ptr *a, const void *v) {
#if defined(__GNUC__) || defined(__clang__)
    void *w = (void *)v;
    __atomic_store((void **)&a->v, &w, __ATOMIC_RELEASE);
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
    a->v = (void *)v;
#else
    a->v = (void *)v;
#endif
}
#endif
