/* sha256.c — FIPS 180-4, byte-oriented, no allocation. */
#include "project/sha256.h"

static uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32u - n));
}

static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static void block(struct RISha256 *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
            ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^
            (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^
            (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0];
    b = s->h[1];
    c = s->h[2];
    d = s->h[3];
    e = s->h[4];
    f = s->h[5];
    g = s->h[6];
    h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t S0, mj;
        t1 = h + S1 + ch + K[i] + w[i];
        S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        mj = (a & b) ^ (a & c) ^ (b & c);
        t2 = S0 + mj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
    s->h[5] += f;
    s->h[6] += g;
    s->h[7] += h;
}

void ri_sha256_init(struct RISha256 *c) {
    c->h[0] = 0x6a09e667u;
    c->h[1] = 0xbb67ae85u;
    c->h[2] = 0x3c6ef372u;
    c->h[3] = 0xa54ff53au;
    c->h[4] = 0x510e527fu;
    c->h[5] = 0x9b05688cu;
    c->h[6] = 0x1f83d9abu;
    c->h[7] = 0x5be0cd19u;
    c->len = 0;
    c->nbuf = 0;
}

void ri_sha256_add(struct RISha256 *c, const void *data, uint32_t n) {
    const unsigned char *p = (const unsigned char *)data;
    uint32_t i;
    for (i = 0; i < n; i++) {
        c->buf[c->nbuf++] = p[i];
        if (c->nbuf == 64u) {
            block(c, c->buf);
            c->nbuf = 0;
        }
    }
    c->len += n;
}

void ri_sha256_end(struct RISha256 *c, unsigned char out[32]) {
    uint64_t bits = c->len * 8u;
    unsigned char pad = 0x80u, zero = 0x00u;
    unsigned char lb[8];
    int i;
    ri_sha256_add(c, &pad, 1);
    while (c->nbuf != 56u)
        ri_sha256_add(c, &zero, 1);
    for (i = 7; i >= 0; i--) {
        lb[i] = (unsigned char)(bits & 0xffu);
        bits >>= 8;
    }
    /* Feed the length block without re-triggering padding: the buffer
     * has exactly 8 free bytes (nbuf == 56), so this fills it once. */
    {
        uint32_t k;
        for (k = 0; k < 8u; k++)
            c->buf[c->nbuf++] = lb[k];
        block(c, c->buf);
        c->nbuf = 0;
    }
    for (i = 0; i < 8; i++) {
        out[4 * i] = (unsigned char)((c->h[i] >> 24) & 0xffu);
        out[4 * i + 1] = (unsigned char)((c->h[i] >> 16) & 0xffu);
        out[4 * i + 2] = (unsigned char)((c->h[i] >> 8) & 0xffu);
        out[4 * i + 3] = (unsigned char)(c->h[i] & 0xffu);
    }
}

void ri_sha256_hex(const void *data, uint32_t n, char hex[65]) {
    struct RISha256 c;
    unsigned char out[32];
    int i;
    ri_sha256_init(&c);
    ri_sha256_add(&c, data, n);
    ri_sha256_end(&c, out);
    for (i = 0; i < 32; i++) {
        hex[2 * i] = "0123456789abcdef"[(out[i] >> 4) & 0xfu];
        hex[2 * i + 1] = "0123456789abcdef"[out[i] & 0xfu];
    }
    hex[64] = '\0';
}
