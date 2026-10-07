/* CPUCOUNT — read-only CPU count for the F3 gate (KrnGetCPUCount).
 * Decides whether the WC trial can cover every CPU from one SuperState
 * window (count==1) or must stop (count>1: no user-mode IPI exists). */
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/kernel.h>

/* kernel.resource base: same owned-data-symbol pattern as BootLoaderBase
 * in memtype.c (the protos resolve through this symbol; the v11 app SDK
 * ships no kernel headers, so proto/kernel.h comes from the generated
 * v11 build-SDK private includes, LVO 40). */
APTR KernelBase = NULL;

int main(void)
{
    unsigned int n = 0;
    struct Library *kres;
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0);
    if (!DOSBase)
        return 20;
    kres = OpenResource("kernel.resource");
    if (kres) {
        KernelBase = kres;
        n = KrnGetCPUCount();
        Printf("CPUCOUNT n=%lu\n", (unsigned long)n);
    } else {
        Printf("CPUCOUNT no-kernel-resource\n");
    }
    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
