/* memtype_sup.s — CPL-0 workers for the MEMTYPE probe (x86-64).
 *
 * Entered via jmp from the kernel's core_Supervisor (arch/x86_64-pc/kernel/
 * core_interrupts.s), NOT via call: there is no return address on the stack,
 * only the CPU interrupt frame. Exit with iret, never ret (same convention
 * as arch/i386-pc/exec/cache.S: Exec_Wbinvd ends in iret).
 *
 * RAX at iret becomes the Supervisor() return value. Integer registers
 * only: the supervisor exit path does not restore XMM/YMM (see the
 * SC_SUPERVISOR comment in arch/all-pc/kernel/cpu_intr.c).
 */

    .text
    .balign 16, 0x90

/* UQUAD mt_sup_rdmsr(void): RDMSR of the MSR number in global mt_msr. */
    .globl  mt_sup_rdmsr
    .type   mt_sup_rdmsr, @function
mt_sup_rdmsr:
    movl    mt_msr(%rip), %ecx
    rdmsr                           /* edx:eax = MSR value */
    shlq    $32, %rdx
    orq     %rdx, %rax
    iret
    .size   mt_sup_rdmsr, .-mt_sup_rdmsr

/* UQUAD mt_sup_cr3(void): returns CR3. */
    .globl  mt_sup_cr3
    .type   mt_sup_cr3, @function
mt_sup_cr3:
    movq    %cr3, %rax
    iret
    .size   mt_sup_cr3, .-mt_sup_cr3
