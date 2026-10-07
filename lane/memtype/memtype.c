/* MEMTYPE — read-only x86 memory-type probe (MTRR / PAT / page walk + VESA FB).
 *
 * ABIv11 (and ABIv1: same source, same recipe family as BLITPROBE).
 * Read-only on the machine: CPUID, RDMSR of the architectural MSRs in §0.3
 * (each gated on its CPUID bit and on VCNT), MOV-from-CR3 and page-table
 * reads. No WRMSR, no CR writes, no page-table writes.
 *
 * Privileged reads run one MSR at a time through Supervisor(): the worker
 * (lane/memtype/memtype_sup.s) returns the 64-bit value in RAX, which is
 * the Supervisor() return value. The MSR number travels in the global
 * mt_msr (a jmp-entered worker takes no parameters).
 *
 * Output is fixed-format printlns; the lane redirects stdout to RAM: and
 * --get's it back. No window is opened.
 */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bootloader.h>
#include <aros/bootloader.h>

#include "memtype_decode.h"

#define MSR_MTRRCAP  0xFEUL
#define MSR_DEF_TYPE 0x2FFUL
#define MSR_PAT      0x277UL
#define MSR_FIX64K   0x250UL
#define MSR_FIX16K_A 0x258UL
#define MSR_FIX16K_B 0x259UL
#define MSR_FIX4K_BASE 0x268UL

#define MAX_MTRR 16

ULONG mt_msr;

/* The bootloader protos use the BootLoaderBase data symbol directly; it is
 * a resource (not an OpenLibrary library), so autoinit provides nothing and
 * the app owns it — assigned from OpenResource() in main, same shape as the
 * vesagfx driver's local of the same name. */
APTR BootLoaderBase = NULL;

extern UQUAD mt_sup_rdmsr(void);
extern UQUAD mt_sup_cr3(void);

/* Callee-saved registers survive Supervisor(): core_Supervisor restores the
 * CPU registers from the saved frame before jumping to the worker, and the
 * worker touches only eax/ecx/edx — but the push/pop pair costs nothing
 * and holds even if that reading ever goes stale. */
static UQUAD sup_rdmsr(ULONG msr)
{
    UQUAD v;
    mt_msr = msr;
    __asm__ __volatile__(
        "pushq %%rbx\n\tpushq %%rbp\n\tpushq %%r12\n\t"
        "pushq %%r13\n\tpushq %%r14\n\tpushq %%r15\n\t"
        : : : "memory");
    v = (UQUAD)Supervisor((APTR)mt_sup_rdmsr);
    __asm__ __volatile__(
        "popq %%r15\n\tpopq %%r14\n\tpopq %%r13\n\t"
        "popq %%r12\n\tpopq %%rbp\n\tpopq %%rbx\n\t"
        : : : "memory");
    return v;
}

static UQUAD sup_cr3(void)
{
    UQUAD v;
    __asm__ __volatile__(
        "pushq %%rbx\n\tpushq %%rbp\n\tpushq %%r12\n\t"
        "pushq %%r13\n\tpushq %%r14\n\tpushq %%r15\n\t"
        : : : "memory");
    v = (UQUAD)Supervisor((APTR)mt_sup_cr3);
    __asm__ __volatile__(
        "popq %%r15\n\tpopq %%r14\n\tpopq %%r13\n\t"
        "popq %%r12\n\tpopq %%rbp\n\tpopq %%rbx\n\t"
        : : : "memory");
    return v;
}

static void cpuid_raw(ULONG leaf, ULONG sub, ULONG *a, ULONG *b, ULONG *c, ULONG *d)
{
    ULONG ea, eb, ec, ed;
    __asm__ __volatile__("cpuid"
        : "=a"(ea), "=b"(eb), "=c"(ec), "=d"(ed)
        : "a"(leaf), "c"(sub));
    *a = ea;
    *b = eb;
    *c = ec;
    *d = ed;
}

static ULONG reg_cs(void)
{
    ULONG cs;
    __asm__ __volatile__("mov %%cs,%0" : "=r"(cs));
    return cs;
}

static void print_u64hex(UQUAD v)
{
    Printf("%08lx%08lx", (unsigned long)(v >> 32), (unsigned long)(v & 0xFFFFFFFFULL));
}

/* Walk PML4 -> PDPT -> PD (-> PT) for one address. Prints one WALK line and
 * returns the PAT index selected (-1 when unmapped above the page). The
 * page size in bytes goes to *psz (0 when unmapped). */
static int walk_one(UQUAD addr, UQUAD cr3, unsigned long *psz)
{
    UQUAD tab, e;
    unsigned long pml4o, pdpto, pdo, pto;
    unsigned long p, rw, us, pwt, pcd, ps, pat;
    *psz = 0;

    pml4o = (unsigned long)((addr >> 39) & 0x1FF);
    pdpto = (unsigned long)((addr >> 30) & 0x1FF);
    pdo   = (unsigned long)((addr >> 21) & 0x1FF);
    pto   = (unsigned long)((addr >> 12) & 0x1FF);

    e = *(volatile UQUAD *)(APTR)(UQUAD)((cr3 & (UQUAD)0xFFFFFFFFFFFFF000ULL) + (UQUAD)pml4o * 8);
    p = (unsigned long)(e & 1); rw = (unsigned long)((e >> 1) & 1);
    us = (unsigned long)((e >> 2) & 1); pwt = (unsigned long)((e >> 3) & 1);
    pcd = (unsigned long)((e >> 4) & 1);
    Printf("WALK addr=");
    print_u64hex(addr);
    Printf(" L4 P=%lu RW=%lu US=%lu PWT=%lu PCD=%lu", p, rw, us, pwt, pcd);
    if (!p) {
        Printf(" unmapped-at-L4\n");
        return -1;
    }
    tab = (e & (UQUAD)0xFFFFFFFFFFFFF000ULL);

    e = *(volatile UQUAD *)(APTR)(UQUAD)(tab + (UQUAD)pdpto * 8);
    p = (unsigned long)(e & 1); rw = (unsigned long)((e >> 1) & 1);
    us = (unsigned long)((e >> 2) & 1); pwt = (unsigned long)((e >> 3) & 1);
    pcd = (unsigned long)((e >> 4) & 1); ps = (unsigned long)((e >> 7) & 1);
    Printf(" L3 P=%lu RW=%lu US=%lu PWT=%lu PCD=%lu PS=%lu", p, rw, us, pwt, pcd, ps);
    if (!p) {
        Printf(" unmapped-at-L3\n");
        return -1;
    }
    if (ps) {
        pat = (unsigned long)((e >> 12) & 1);
        *psz = 1024UL * 1024UL * 1024UL;
        Printf(" page=1G PAT12=%lu idx=%d\n", pat, mt_pat_index((int)pwt, (int)pcd, (int)pat));
        return mt_pat_index((int)pwt, (int)pcd, (int)pat);
    }
    tab = (e & (UQUAD)0xFFFFFFFFFFFFF000ULL);

    e = *(volatile UQUAD *)(APTR)(UQUAD)(tab + (UQUAD)pdo * 8);
    p = (unsigned long)(e & 1); rw = (unsigned long)((e >> 1) & 1);
    us = (unsigned long)((e >> 2) & 1); pwt = (unsigned long)((e >> 3) & 1);
    pcd = (unsigned long)((e >> 4) & 1); ps = (unsigned long)((e >> 7) & 1);
    Printf(" L2 P=%lu RW=%lu US=%lu PWT=%lu PCD=%lu PS=%lu", p, rw, us, pwt, pcd, ps);
    if (!p) {
        Printf(" unmapped-at-L2\n");
        return -1;
    }
    if (ps) {
        pat = (unsigned long)((e >> 12) & 1);
        *psz = 1024UL * 1024UL * 2UL;
        Printf(" page=2M PAT12=%lu idx=%d\n", pat, mt_pat_index((int)pwt, (int)pcd, (int)pat));
        return mt_pat_index((int)pwt, (int)pcd, (int)pat);
    }
    tab = (e & (UQUAD)0xFFFFFFFFFFFFF000ULL);

    e = *(volatile UQUAD *)(APTR)(UQUAD)(tab + (UQUAD)pto * 8);
    p = (unsigned long)(e & 1); rw = (unsigned long)((e >> 1) & 1);
    us = (unsigned long)((e >> 2) & 1); pwt = (unsigned long)((e >> 3) & 1);
    pcd = (unsigned long)((e >> 4) & 1); pat = (unsigned long)((e >> 7) & 1);
    Printf(" L1 P=%lu RW=%lu US=%lu PWT=%lu PCD=%lu PAT7=%lu", p, rw, us, pwt, pcd, pat);
    if (!p) {
        Printf(" unmapped-at-L1\n");
        return -1;
    }
    *psz = 4096UL;
    Printf(" page=4K idx=%d\n", mt_pat_index((int)pwt, (int)pcd, (int)pat));
    return mt_pat_index((int)pwt, (int)pcd, (int)pat);
}

int main(void)
{
    ULONG a, b, c, d, maxext = 0;
    ULONG cpl, has_mtrr = 0, has_pat = 0;
    ULONG vcnt = 0, fix = 0, wc = 0;
    ULONG deftype = 0, fe = 0, en = 0;
    UQUAD cap = 0, def = 0, patraw = 0, cr3 = 0;
    UQUAD bases[MAX_MTRR], masks[MAX_MTRR];
    int maxphy = 0, i, n;
    int mtrr_t, pat_t, eff;
    char vendor[13], brand[49];
    struct VesaInfo *vi = NULL;
    UQUAD fb = 0, fbsz = 0, fbend, fbmid;
    unsigned long psz = 0;
    int idx0, idx1, idx2, k;
    int match[16];
    struct Library *blres;

    for (i = 0; i < MAX_MTRR; i++) {
        bases[i] = 0;
        masks[i] = 0;
    }

    Printf("MEMTYPE v1\n");
    cpl = reg_cs() & 3u;
    Printf("CPL=%lu\n", cpl);

    cpuid_raw(0, 0, &a, &b, &c, &d);
    vendor[0] = (char)(b & 0xFF); vendor[1] = (char)((b >> 8) & 0xFF);
    vendor[2] = (char)((b >> 16) & 0xFF); vendor[3] = (char)((b >> 24) & 0xFF);
    vendor[4] = (char)(d & 0xFF); vendor[5] = (char)((d >> 8) & 0xFF);
    vendor[6] = (char)((d >> 16) & 0xFF); vendor[7] = (char)((d >> 24) & 0xFF);
    vendor[8] = (char)(c & 0xFF); vendor[9] = (char)((c >> 8) & 0xFF);
    vendor[10] = (char)((c >> 16) & 0xFF); vendor[11] = (char)((c >> 24) & 0xFF);
    vendor[12] = 0;
    cpuid_raw(1, 0, &a, &b, &c, &d);
    has_mtrr = (d >> 12) & 1u;
    has_pat = (d >> 16) & 1u;
    Printf("CPU vendor=%s fam=%lu mod=%lu step=%lu mtrr=%lu pat=%lu\n",
        vendor, (a >> 8) & 15u, (a >> 4) & 15u, a & 15u, has_mtrr, has_pat);

    cpuid_raw(0x80000000UL, 0, &a, &b, &c, &d);
    maxext = a;
    if (maxext >= 0x80000004UL) {
        for (i = 0; i < 3; i++) {
            ULONG r[4];
            int k2;
            cpuid_raw(0x80000002UL + (ULONG)i, 0, &r[0], &r[1], &r[2], &r[3]);
            for (k2 = 0; k2 < 4; k2++) {
                brand[i * 16 + k2 * 4 + 0] = (char)(r[k2] & 0xFFu);
                brand[i * 16 + k2 * 4 + 1] = (char)((r[k2] >> 8) & 0xFFu);
                brand[i * 16 + k2 * 4 + 2] = (char)((r[k2] >> 16) & 0xFFu);
                brand[i * 16 + k2 * 4 + 3] = (char)((r[k2] >> 24) & 0xFFu);
            }
        }
        brand[48] = 0;
        Printf("CPU brand=%s\n", brand);
    } else {
        Printf("CPU brand=(unavailable)\n");
    }
    if (maxext >= 0x80000008UL) {
        cpuid_raw(0x80000008UL, 0, &a, &b, &c, &d);
        maxphy = (int)(a & 0xFFu);
    }
    Printf("MAXPHYADDR=%d\n", maxphy);

    if (has_mtrr && maxphy >= 12) {
        int nn;
        cap = sup_rdmsr(MSR_MTRRCAP);
        vcnt = (unsigned long)(cap & 0xFFu);
        fix = (unsigned long)((cap >> 8) & 1u);
        wc = (unsigned long)((cap >> 10) & 1u);
        Printf("MTRRCAP raw=");
        print_u64hex(cap);
        Printf(" VCNT=%lu FIX=%lu WC=%lu\n", vcnt, fix, wc);
        def = sup_rdmsr(MSR_DEF_TYPE);
        deftype = (unsigned long)(def & 0xFFu);
        fe = (unsigned long)((def >> 10) & 1u);
        en = (unsigned long)((def >> 11) & 1u);
        Printf("MTRR_DEF raw=");
        print_u64hex(def);
        Printf(" deftype=%s(%lu) FE=%lu E=%lu\n", mt_name(mt_mtrr_type(def)), deftype, fe, en);
        nn = (vcnt < MAX_MTRR) ? (int)vcnt : MAX_MTRR;
        for (i = 0; i < nn; i++) {
            UQUAD ba = sup_rdmsr(0x200UL + (ULONG)(i * 2));
            UQUAD ma = sup_rdmsr(0x201UL + (ULONG)(i * 2));
            bases[i] = ba;
            masks[i] = ma;
            Printf("MTRR%lu base=", (unsigned long)i);
            print_u64hex(ba);
            Printf(" mask=");
            print_u64hex(ma);
            if (mt_mtrr_valid(ma)) {
                UQUAD bs = mt_mtrr_base(ba, ma);
                UQUAD sz = mt_mtrr_size(ma, maxphy);
                Printf(" V=1 [");
                print_u64hex(bs);
                Printf(",");
                print_u64hex(bs + sz);
                Printf(") %s", mt_name(mt_mtrr_type(ba)));
            } else {
                Printf(" V=0");
            }
            Printf("\n");
        }
        if (fix) {
            UQUAD f;
            f = sup_rdmsr(MSR_FIX64K);
            Printf("FIX64K=");
            print_u64hex(f);
            Printf("\n");
            f = sup_rdmsr(MSR_FIX16K_A);
            Printf("FIX16K_A=");
            print_u64hex(f);
            Printf("\n");
            f = sup_rdmsr(MSR_FIX16K_B);
            Printf("FIX16K_B=");
            print_u64hex(f);
            Printf("\n");
            for (i = 0; i < 8; i++) {
                f = sup_rdmsr(MSR_FIX4K_BASE + (ULONG)i);
                Printf("FIX4K_%lu=", (unsigned long)i);
                print_u64hex(f);
                Printf("\n");
            }
        }
        n = nn;
    } else {
        Printf("MTRR unsupported-or-no-maxphy (no RDMSR attempted)\n");
        n = 0;
    }

    if (has_pat) {
        int j;
        patraw = sup_rdmsr(MSR_PAT);
        Printf("PAT raw=");
        print_u64hex(patraw);
        for (j = 0; j < 8; j++)
            Printf(" PA%d=%s", j, mt_name(mt_pat_entry(patraw, j)));
        Printf("\n");
    } else {
        Printf("PAT unsupported (no RDMSR attempted)\n");
    }

    blres = OpenResource("bootloader.resource");
    if (blres) {
        BootLoaderBase = blres;
        vi = (struct VesaInfo *)GetBootInfo(BL_Video);
    }
    if (vi && vi->FrameBuffer) {
        fb = (UQUAD)(APTR)vi->FrameBuffer;
        fbsz = (UQUAD)vi->FrameBufferSize * 1024ULL;
        Printf("FB source=bootloader base=");
        print_u64hex(fb);
        Printf(" size=%lu pci=not-queried\n", (unsigned long)(fbsz & 0xFFFFFFFFULL));
    } else {
        Printf("FB source=unknown (no bootloader VESA info)\n");
    }

    if (fb && has_mtrr && has_pat && maxphy >= 12) {
        fbend = fb + fbsz - 1;
        fbmid = fb + (fbsz / 2) - ((fbsz / 2) % (2UL * 1024UL * 1024UL));
        cr3 = sup_cr3();
        Printf("CR3=");
        print_u64hex(cr3);
        Printf("\n");
        idx0 = walk_one(fb, cr3, &psz);
        idx1 = walk_one(fbmid, cr3, &psz);
        idx2 = walk_one(fbend, cr3, &psz);
        /* MTRR type at fb, fbmid, fbend: all three must agree. */
        for (k = 0; k < 3; k++) {
            UQUAD ad = (k == 0) ? fb : ((k == 1) ? fbmid : fbend);
            int nm = 0, q;
            for (q = 0; q < n; q++) {
                if (!mt_mtrr_valid(masks[q]))
                    continue;
                {
                    UQUAD bs = mt_mtrr_base(bases[q], masks[q]);
                    UQUAD sz = mt_mtrr_size(masks[q], maxphy);
                    if (sz && ad >= bs && ad < bs + sz && nm < 16)
                        match[nm++] = mt_mtrr_type(bases[q]);
                }
            }
            if (!en)
                match[nm++] = MT_UC; /* MTRRs disabled: everything UC */
            else if (nm == 0)
                match[nm++] = (int)(def & 0xFFu);
            Printf("MTRR_AT_%d=", k);
            print_u64hex(ad);
            if (nm == 1) {
                Printf(" %s", mt_name(match[0]));
            } else {
                Printf(" overlap%d=%s", nm, mt_name(mt_mtrr_overlap(match, nm)));
            }
            Printf("\n");
            if (k == 0)
                mtrr_t = (nm == 1) ? match[0] : mt_mtrr_overlap(match, nm);
            else {
                int tt = (nm == 1) ? match[0] : mt_mtrr_overlap(match, nm);
                if (tt != mtrr_t)
                    Printf("NOTE MTRR type differs across FB range\n");
            }
        }
        pat_t = (idx0 >= 0) ? mt_pat_entry(patraw, idx0) : MT_UNKNOWN;
        if (idx1 != idx0 || idx2 != idx0)
            Printf("NOTE PAT index differs across FB range (%d/%d/%d)\n", idx0, idx1, idx2);
        eff = mt_effective(pat_t, mtrr_t);
        Printf("FB effective=%s via MTRR=%s PAT[%d]=%s\n",
            mt_name(eff), mt_name(mtrr_t), idx0, mt_name(pat_t));
    } else {
        Printf("FB effective=unknown (missing FB, MTRR, PAT or MAXPHYADDR)\n");
    }
    return 0;
}
