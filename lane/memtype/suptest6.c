/* SUPTEST6 — SuperState()/UserState() probe: run RDMSR at CPL0 inside the
 * documented window, with direct inline asm (no iret workers). */
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>

static UQUAD raw_rdmsr(ULONG msr)
{
    ULONG lo, hi;
    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((UQUAD)hi << 32) | (UQUAD)lo;
}

int main(void)
{
    APTR ssp;
    UQUAD cap = 0, cr3v = 0;
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0);
    if (!DOSBase)
        return 20;
    Printf("SUPTEST6 calling SuperState...\n");
    ssp = SuperState();
    if (ssp) {
        cap = raw_rdmsr(0xFEUL);
        __asm__ __volatile__("mov %%cr3,%0" : "=r"(cr3v));
        UserState(ssp);
        Printf("SUPTEST6 super mtrrcap=%08lx%08lx cr3=%08lx%08lx done\n",
            (unsigned long)(cap >> 32), (unsigned long)(cap & 0xFFFFFFFFULL),
            (unsigned long)(cr3v >> 32), (unsigned long)(cr3v & 0xFFFFFFFFULL));
    } else {
        Printf("SUPTEST6 already-super (CPL0), no window needed\n");
        cap = raw_rdmsr(0xFEUL);
        Printf("SUPTEST6 direct mtrrcap=%08lx%08lx done\n",
            (unsigned long)(cap >> 32), (unsigned long)(cap & 0xFFFFFFFFULL));
    }
    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
