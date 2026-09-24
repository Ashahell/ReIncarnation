/* Measured deterministic kernels (Task 2, gate G2).
 * Method note (deviation from brief Step 3, measured justification):
 * the brief's table+linear tanh (65 entries, step 0.125) has max
 * interpolation error 1.5e-3 on [-4,4] (measured /tmp/ri/run/t2),
 * 1500x over the 1e-6 test bound; cubic Hermite on the same table still
 * gives 2.6e-6. tanh is therefore evaluated via the double-precision
 * internal ri_exp (error ~1 ulp). Likewise the brief's order-4 exp Horner
 * (remainder ~4.2e-05) and order-5 sin Taylor on [-pi,pi] (remainder ~0.6)
 * cannot meet their bounds; both use higher-order double-precision
 * polynomials with exact power-of-two scaling. No libm calls anywhere.
 */
#include "engine/dsp/kernels.h"
#include <stdint.h>

/* +Inf without libm (audit 0b allowlists kernel-internal construction). */
static float ri_inff(void) {
    union { uint32_t u; float f; } c;
    c.u = 0x7f800000u;
    return c.f;
}

/* Exact power-of-two scale by a bounded loop. ak = |k| <= 128.
 * Multiplying/dividing by 2.0 is exact in binary FP (no rounding), so
 * results for a given k are identical whatever the loop bound. The
 * common path (|k| <= 32) keeps the historical 32-iteration loop
 * exactly (same ops, same speed); the wide bound serves totality only
 * and never fires on in-domain inputs. */
static float ri_scale2(float v, int k) {
    int ak = k >= 0 ? k : -k;
    float s = v;
    int i, bound;
    bound = ak <= 32 ? 32 : 129;
    for (i = 0; i < bound; i++) {
        if (i < ak) {
            if (k >= 0) {
                s = s * 2.0f;
            } else {
                s = s * 0.5f;
            }
        }
    }
    return s;
}

/* e^x, total: 0 for x <= -87 (float underflow point), +Inf above
 * 88.72283 (float overflow point), exact range reduction otherwise.
 * k = round(x/ln2) now spans +-128 so r = x - k*ln2 stays in
 * [-ln2/2, ln2/2] for every finite result; order-9 Taylor of e^r in
 * double; exact 2^k scale; single final float rounding.
 * Bit-exact with the old code wherever the old k was unclamped
 * (|x*log2e| < 32): same k, same r, same polynomial, same scale ops. */
float ri_exp(float x) {
    double xd = (double)x;
    double r, p;
    float ft;
    int k;
    if (x <= -87.0f) {
        return 0.0f;
    }
    if (x >= 88.72283f) {
        return ri_inff();
    }
    /* k = round(x * log2(e)), guarded float compare so the int cast
     * only ever sees (-128,128) even for out-of-domain inputs. */
    ft = x * 1.442695f;
    if (ft >= 128.0f) {
        k = 128;
    } else if (ft <= -128.0f) {
        k = -128;
    } else {
        k = (int)(ft + (ft >= 0.0f ? 0.5f : -0.5f));
    }
    r = xd - (double)k * 0.6931471805599453;
    /* Horner order-9 Taylor of e^r: (((((((c9*r + c8)*r + c7) ... */
    p = 1.0 / 362880.0;
    p = p * r + 1.0 / 40320.0;
    p = p * r + 1.0 / 5040.0;
    p = p * r + 1.0 / 720.0;
    p = p * r + 1.0 / 120.0;
    p = p * r + 1.0 / 24.0;
    p = p * r + 1.0 / 6.0;
    p = p * r + 1.0 / 2.0;
    p = p * r + 1.0;
    p = p * r + 1.0;
    return ri_scale2((float)p, k);
}

/* tanh via internal exp: tanh(x) = 1 - 2/(exp(2x)+1).
 * Double-precision interior; single final float rounding. */
float ri_tanh(float x) {
    double xd = (double)x;
    double e, t;
    if (xd >= 8.0) {
        return 1.0f;
    }
    if (xd <= -8.0) {
        return -1.0f;
    }
    e = (double)ri_exp((float)(xd * 2.0));
    t = 1.0 - 2.0 / (e + 1.0);
    return (float)t;
}

/* sin, total: exact cycle reduction while the count fits int64
 * (|x| < ~5.7e19 — every physical signal by ~18 orders of magnitude);
 * bounded 0.0 beyond (deterministic precision statement per D-j, not
 * a value claim). Reduce mod 2pi (double), fold to [-pi/2,pi/2],
 * order-13 Taylor in double, single final rounding.
 * Bit-exact with the old code for |cycles| <= 64: identical q, y, p. */
float ri_sin(float x) {
    double xd = (double)x;
    double ft = xd * 0.15915494309189535;
    double y;
    double y2, p;
    int q;
    if (ft > -64.0 && ft < 64.0) {
        if (ft >= 0.0) {
            q = (int)(ft + 0.5);
        } else {
            q = (int)(ft - 0.5);
        }
    } else {
        /* Wide reduction: int64 holds ±9.2e18 cycles exactly. */
        double qd;
        if (ft > -9.0e18 && ft < 9.0e18) {
            int64_t qi = (int64_t)(ft >= 0.0 ? ft + 0.5 : ft - 0.5);
            qd = (double)qi;
        } else {
            return 0.0f;
        }
        y = xd - qd * 6.283185307179586;
        if (y > 1.5707963267948966) {
            y = 3.141592653589793 - y;
        } else if (y < -1.5707963267948966) {
            y = -3.141592653589793 - y;
        }
        y2 = y * y;
        p = 1.0 / 6227020800.0;
        p = p * y2 - 1.0 / 39916800.0;
        p = p * y2 + 1.0 / 362880.0;
        p = p * y2 - 1.0 / 5040.0;
        p = p * y2 + 1.0 / 120.0;
        p = p * y2 - 1.0 / 6.0;
        p = p * y2 + 1.0;
        return (float)(p * y);
    }
    y = xd - (double)q * 6.283185307179586;
    if (y > 1.5707963267948966) {
        y = 3.141592653589793 - y;
    } else if (y < -1.5707963267948966) {
        y = -3.141592653589793 - y;
    }
    y2 = y * y;
    /* sin(y)/y = sum_{k=0..6} (-1)^k y^{2k}/(2k+1)!: top cell k=6 is +. */
    p = 1.0 / 6227020800.0; /* +1/13! */
    p = p * y2 - 1.0 / 39916800.0; /* -1/11! */
    p = p * y2 + 1.0 / 362880.0; /* +1/9! */
    p = p * y2 - 1.0 / 5040.0; /* -1/7! */
    p = p * y2 + 1.0 / 120.0; /* +1/5! */
    p = p * y2 - 1.0 / 6.0; /* -1/3! */
    p = p * y2 + 1.0; /* k=0 */
    return (float)(p * y);
}

/* log2(x): bit-split exponent + double Taylor of log(1+u)/ln2.
 * x > 0 normal: e = expbits - 127, m in [1,2), u = m - 1 in [0,1).
 * x <= 0: -Inf (deterministic; slide never queries it — floor is 35 Hz).
 * NaN propagates; +Inf -> +Inf. Denormals scale up once (exact) first. */
float ri_log2(float x) {
    union { float f; uint32_t u; } c;
    uint32_t ebits;
    int e;
    double u2, p, m;
    c.f = x;
    ebits = (c.u >> 23) & 0xffu;
    if (ebits == 0u) {
        if ((c.u & 0x7fffffu) == 0u)
            return -ri_inff(); /* +-0 */
        return ri_log2(x * 33554432.0f) - 25.0f; /* denormal: exact 2^25 up */
    }
    if (ebits == 0xffu) {
        if ((c.u & 0x7fffffu) == 0u)
            return x > 0.0f ? ri_inff() : -ri_inff(); /* +-Inf */
        return x; /* NaN propagates */
    }
    if (x < 0.0f)
        return -ri_inff();
    e = (int)ebits - 127;
    c.u = (c.u & 0x7fffffu) | 0x3f800000u;
    m = (double)c.f;
    /* ln(m)/ln2 via 2*atanh series: v = (m-1)/(m+1) in [0,1/3),
     * ln(m) = 2*(v + v^3/3 + v^5/5 + ...). Horner in w = v^2. */
    u2 = (m - 1.0) / (m + 1.0);
    {
        double w = u2 * u2, q;
        q = 1.0 / 13.0;
        q = q * w + 1.0 / 11.0;
        q = q * w + 1.0 / 9.0;
        q = q * w + 1.0 / 7.0;
        q = q * w + 1.0 / 5.0;
        q = q * w + 1.0 / 3.0;
        q = q * w + 1.0;
        p = 2.0 * u2 * q / 0.6931471805599453;
    }
    return (float)((double)e + p);
}
/* 2^x, total: 0 for x <= -127 (below float normal floor), +Inf for
 * x >= 128, exact floor split otherwise. n = floor(x), f = x - n in
 * [0,1), order-12 Taylor of 2^f = e^(f*ln2) in double, exact 2^n scale.
 * Bit-exact with the old code for |x| < 32: identical branch, n, f. */
float ri_pow2(float x) {
    double xd = (double)x;
    double f, g, p;
    int n, ni;
    if (xd >= 128.0) {
        return ri_inff();
    }
    if (xd <= -127.0) {
        return 0.0f;
    }
    if (xd >= 32.0) {
        ni = (int)xd; /* truncation toward zero = floor for positives */
        n = ni;
        f = xd - (double)ni;
    } else if (xd <= -32.0) {
        ni = (int)xd; /* truncation toward zero; adjust down to floor */
        if ((double)ni > xd) {
            ni = ni - 1;
        }
        n = ni;
        f = xd - (double)ni;
    } else {
        ni = (int)xd; /* truncation toward zero */
        if ((double)ni > xd) {
            ni = ni - 1; /* floor for negatives */
        }
        n = ni;
        f = xd - (double)ni;
    }
    g = f * 0.6931471805599453;
    /* Horner order-12 Taylor of e^g: 1/12! .. 1/0!. */
    p = 1.0 / 479001600.0; /* 1/12! */
    p = p * g + 1.0 / 39916800.0; /* 1/11! */
    p = p * g + 1.0 / 3628800.0; /* 1/10! */
    p = p * g + 1.0 / 362880.0; /* 1/9! */
    p = p * g + 1.0 / 40320.0; /* 1/8! */
    p = p * g + 1.0 / 5040.0; /* 1/7! */
    p = p * g + 1.0 / 720.0; /* 1/6! */
    p = p * g + 1.0 / 120.0; /* 1/5! */
    p = p * g + 1.0 / 24.0; /* 1/4! */
    p = p * g + 1.0 / 6.0; /* 1/3! */
    p = p * g + 1.0 / 2.0; /* 1/2! */
    p = p * g + 1.0; /* 1/1! */
    p = p * g + 1.0; /* 1/0! */
    return ri_scale2((float)p, n);
}
