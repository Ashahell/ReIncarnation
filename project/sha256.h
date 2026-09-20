/* sha256.h — minimal SHA-256 (Task 13, gate G13).
 * Content identity for RBNG/RBNM (spec §13: SHA-256 = identity, CRC =
 * corruption detection). No allocation, no libm, host + AROS clean.
 * FIPS 180-4.
 */
#ifndef RI_SHA256_H
#define RI_SHA256_H
#include <stdint.h>

struct RISha256 {
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    uint32_t nbuf;
};

void ri_sha256_init(struct RISha256 *c);
void ri_sha256_add(struct RISha256 *c, const void *data, uint32_t n);
void ri_sha256_end(struct RISha256 *c, unsigned char out[32]);

/* One-shot hex digest (64 lowercase chars + NUL). */
void ri_sha256_hex(const void *data, uint32_t n, char hex[65]);
#endif
