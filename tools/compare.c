/* tools/compare — event-diff + WAV SHA-256 diff, single source of truth
 * (Task 4, gate G4; spec §13/§16 T3). Compares two event dumps byte-exact
 * and two WAV payloads sample-exact, prints SHA-256 + error metrics.
 *
 * usage: compare --events-a A --events-b B --wav-a C --wav-b D
 * exit: 0 identical, 1 any difference, 2 usage/IO error.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- minimal SHA-256 (public algorithm, written from the spec) ---- */
struct SHA256 {
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    uint32_t nbuf;
};

static uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32u - n));
}

static void sha256_block(struct SHA256 *s, const unsigned char *p) {
    static const uint32_t K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
            ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
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
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
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

static void sha256_init(struct SHA256 *s) {
    s->h[0] = 0x6a09e667u;
    s->h[1] = 0xbb67ae85u;
    s->h[2] = 0x3c6ef372u;
    s->h[3] = 0xa54ff53au;
    s->h[4] = 0x510e527fu;
    s->h[5] = 0x9b05688cu;
    s->h[6] = 0x1f83d9abu;
    s->h[7] = 0x5be0cd19u;
    s->len = 0;
    s->nbuf = 0;
}

static void sha256_update(struct SHA256 *s, const unsigned char *p, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        s->buf[s->nbuf++] = p[i];
        s->len++;
        if (s->nbuf == 64) {
            sha256_block(s, s->buf);
            s->nbuf = 0;
        }
    }
}

static void sha256_final(struct SHA256 *s, unsigned char out[32]) {
    uint64_t bits = s->len * 8u;
    int i;
    unsigned char pad = 0x80;
    unsigned char zero = 0x00;
    unsigned char lb[8];
    sha256_update(s, &pad, 1);
    while (s->nbuf != 56)
        sha256_update(s, &zero, 1);
    for (i = 0; i < 8; i++)
        lb[i] = (unsigned char)((bits >> (56 - 8 * i)) & 0xffu);
    sha256_update(s, lb, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)((s->h[i] >> 24) & 0xffu);
        out[i * 4 + 1] = (unsigned char)((s->h[i] >> 16) & 0xffu);
        out[i * 4 + 2] = (unsigned char)((s->h[i] >> 8) & 0xffu);
        out[i * 4 + 3] = (unsigned char)(s->h[i] & 0xffu);
    }
}

static void print_hex(const unsigned char *p, int n) {
    int i;
    for (i = 0; i < n; i++)
        printf("%02x", p[i]);
}

/* ---- file helpers ---- */

/* Read a whole file into a static buffer (bounded 2^24). *err set on error. */
static uint32_t read_file(const char *path, unsigned char *dst, uint32_t cap, int *err) {
    FILE *f = fopen(path, "rb");
    uint32_t n = 0;
    int ch;
    *err = 0;
    if (!f) {
        printf("compare: cannot open %s\n", path);
        *err = 1;
        return 0;
    }
    while ((ch = fgetc(f)) != EOF) {
        if (n >= cap) {
            printf("compare: file too large %s\n", path);
            fclose(f);
            *err = 1;
            return 0;
        }
        dst[n++] = (unsigned char)ch;
    }
    fclose(f);
    return n;
}

static uint32_t rd32le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Split a WAV into (payload pointer, payload bytes). -1 on format error. */
static int wav_payload(unsigned char *buf, uint32_t n, unsigned char **pay, uint32_t *npay) {
    uint32_t rate, ch, bits;
    if (n < 44 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVEfmt ", 8) != 0 ||
        memcmp(buf + 36, "data", 4) != 0)
        return -1;
    ch = buf[22] | ((uint32_t)buf[23] << 8);
    rate = rd32le(buf + 24);
    bits = buf[34] | ((uint32_t)buf[35] << 8);
    if (ch != 1 || rate != 48000u || bits != 16)
        return -1;
    *npay = rd32le(buf + 40);
    if (*npay + 44 > n)
        return -1;
    *pay = buf + 44;
    return 0;
}

int main(int argc, char **argv) {
    const char *eva = NULL, *evb = NULL, *wava = NULL, *wavb = NULL;
    static unsigned char fa[16777216], fb[16777216];
    unsigned char *pa, *pb;
    uint32_t na, nb, npa, npb, i;
    unsigned char ha[32], hb[32];
    struct SHA256 s;
    int differ = 0;
    int k, ea, eb, wa, wb;
    for (k = 1; k < argc; k++) {
        if (strcmp(argv[k], "--events-a") == 0 && k + 1 < argc)
            eva = argv[++k];
        else if (strcmp(argv[k], "--events-b") == 0 && k + 1 < argc)
            evb = argv[++k];
        else if (strcmp(argv[k], "--wav-a") == 0 && k + 1 < argc)
            wava = argv[++k];
        else if (strcmp(argv[k], "--wav-b") == 0 && k + 1 < argc)
            wavb = argv[++k];
        else {
            printf("usage: compare --events-a A --events-b B --wav-a C --wav-b D\n");
            return 2;
        }
    }
    if (!eva || !evb || !wava || !wavb) {
        printf("usage: compare --events-a A --events-b B --wav-a C --wav-b D\n");
        return 2;
    }
    /* events: byte-exact text compare */
    na = read_file(eva, fa, sizeof fa, &ea);
    nb = read_file(evb, fb, sizeof fb, &eb);
    if (ea || eb)
        return 2;
    if (na != nb) {
        printf("events DIFFER: sizes %u vs %u\n", na, nb);
        differ = 1;
    } else {
        uint32_t first = 0;
        for (i = 0; i < na; i++) {
            if (fa[i] != fb[i]) {
                first = i;
                break;
            }
        }
        if (i < na) {
            uint32_t line = 1, ls = 0;
            for (i = 0; i < first; i++)
                if (fa[i] == '\n') {
                    line++;
                    ls = i + 1;
                }
            printf("events DIFFER at byte %u (line %u):\n  a: %.*s\n  b: %.*s\n", first, line,
                60, fa + ls, 60, fb + ls);
            differ = 1;
        } else {
            printf("events identical (%u bytes)\n", na);
        }
    }
    /* wav: payload compare + SHA-256 + metrics */
    na = read_file(wava, fa, sizeof fa, &wa);
    nb = read_file(wavb, fb, sizeof fb, &wb);
    if (wa || wb)
        return 2;
    if (wav_payload(fa, na, &pa, &npa) != 0) {
        printf("wav-a bad format %s\n", wava);
        return 2;
    }
    if (wav_payload(fb, nb, &pb, &npb) != 0) {
        printf("wav-b bad format %s\n", wavb);
        return 2;
    }
    sha256_init(&s);
    sha256_update(&s, pa, npa);
    sha256_final(&s, ha);
    sha256_init(&s);
    sha256_update(&s, pb, npb);
    sha256_final(&s, hb);
    printf("wav-a sha256: ");
    print_hex(ha, 32);
    printf("\nwav-b sha256: ");
    print_hex(hb, 32);
    printf("\n");
    if (npa != npb) {
        printf("wav DIFFER: payload sizes %u vs %u\n", npa, npb);
        differ = 1;
    } else {
        uint32_t nmis = 0;
        uint32_t maxabs = 0;
        for (i = 0; i + 1 < npa; i += 2) {
            int16_t xa = (int16_t)((uint16_t)pa[i] | ((uint16_t)pa[i + 1] << 8));
            int16_t xb = (int16_t)((uint16_t)pb[i] | ((uint16_t)pb[i + 1] << 8));
            uint32_t d = (xa >= xb) ? (uint32_t)(xa - xb) : (uint32_t)(xb - xa);
            if (d != 0)
                nmis++;
            if (d > maxabs)
                maxabs = d;
        }
        if (nmis == 0)
            printf("wav identical (%u samples)\n", npa / 2u);
        else {
            printf("wav DIFFER: %u/%u samples differ, max-abs %u LSB\n", nmis, npa / 2u, maxabs);
            differ = 1;
        }
    }
    if (differ) {
        printf("COMPARE: DIFFERENT\n");
        return 1;
    }
    printf("COMPARE: IDENTICAL\n");
    return 0;
}
