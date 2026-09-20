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

/* Exact power-of-two scale by a bounded loop. ak = |k| <= 32.
 * Multiplying/dividing by 2.0 is exact in binary FP (no rounding). */
static float ri_scale2(float v, int k) {
    int ak = k >= 0 ? k : -k;
    float s = v;
    int i;
    for (i = 0; i < 32; i++) {
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

/* e^x on [-8,8]. k = round(x/ln2) clamped +-32; r = x - k*ln2 in
 * [-ln2/2, ln2/2]; order-9 Taylor of e^r in double; exact 2^k scale;
 * single final float rounding. */
float ri_exp(float x) {
    double xd = (double)x;
    double r, p;
    float ft;
    int k;
    /* k = round(x * log2(e)), guarded float compare so the int cast
     * only ever sees (-32,32) even for out-of-domain inputs. */
    ft = x * 1.442695f;
    if (ft >= 32.0f) {
        k = 32;
    } else if (ft <= -32.0f) {
        k = -32;
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

/* sin with cycle clamp +-64. Reduce mod 2pi (double), fold to
 * [-pi/2,pi/2], order-13 Taylor in double, single final rounding. */
float ri_sin(float x) {
    double xd = (double)x;
    double ft = xd * 0.15915494309189535;
    double y;
    double y2, p;
    int q;
    if (ft >= 64.0) {
        q = 64;
    } else if (ft <= -64.0) {
        q = -64;
    } else {
        q = (int)(ft + (ft >= 0.0 ? 0.5 : -0.5));
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

/* 2^x on [-32,32]. n = floor(x) clamped +-32, f = x - n in [0,1),
 * order-12 Taylor of 2^f = e^(f*ln2) in double, exact 2^n scale. */
float ri_pow2(float x) {
    double xd = (double)x;
    double f, g, p;
    int n, ni;
    if (xd >= 32.0) {
        n = 32;
        f = 0.0;
    } else if (xd <= -32.0) {
        n = -32;
        f = 0.0;
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
