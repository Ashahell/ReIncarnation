/* timerres.c - how long does a 1 ms TR_ADDREQUEST on timer.device
 * UNIT_MICROHZ actually take on this machine?
 *
 * R12 N3: the clock-out sender pumps every "1 ms" tick. The host sees
 * 56.00 clocks/s (correct rate) but in bursts. This measures the real pump
 * interval instead of guessing it.
 *
 * MEASURED (Dell e6320, 2026-10-19): n=200 min=1019 mean=1977 max=7928 us.
 * So the tick is ~2 ms, NOT a VBlank frame, and it is not the source of the
 * 16/21 ms spacing -- that is the producer's 256-frame block quantisation
 * (see docs/evidence/midi/r12/r12-n3-clockout.md).
 *
 * Prints 200 waits: per-iteration EClock microseconds, min/mean/max.
 *
 * build (ABIv11, the same flags as scripts/ri_build_v11.sh):
 *   VK=~/Work/projects/Vulkan4Aros
 *   SDK=$VK/src/abi/v11/sdk/Developer
 *   PATH=$VK/src/abi/v11/toolchain-core-x86_64:$PATH
 *   x86_64-aros-gcc -std=gnu99 -O2 -mcmodel=large -mno-red-zone \
 *     -fno-strict-aliasing -ffixed-r12 -fno-builtin -I$SDK/include \
 *     -c tools/timerres.c -o timerres.o
 *   x86_64-aros-gcc -mcmodel=large -mno-red-zone -ffixed-r12 \
 *     -nostartfiles -no-pie -o TIMERRES timerres.o $SDK/lib/startup.o \
 *     -L$SDK/lib -lamiga -ldos -lexec -lautoinit
 */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <devices/timer.h>
#include <stdint.h>
#include <stdio.h>

/* proto/timer.h's inline ReadEClock goes through this base. */
struct Device *TimerBase;

static uint64_t eclk_us(struct EClockVal *ev, ULONG freq)
{
    uint64_t t = ((uint64_t)ev->ev_hi << 32) | (uint64_t)ev->ev_lo;
    return t / freq * 1000000u + (t % freq) * 1000000u / freq;
}

int main(void)
{
    struct MsgPort *p;
    struct timerequest *tr;
    struct EClockVal e0, e1;
    ULONG freq;
    uint64_t min = ~0ull, max = 0, sum = 0;
    int i, n = 200;
    FILE *f;

    p = CreateMsgPort();
    if (!p)
        return 1;
    tr = (struct timerequest *)CreateIORequest(p, sizeof(*tr));
    if (!tr)
        return 1;
    if (OpenDevice((STRPTR)"timer.device", UNIT_MICROHZ,
            (struct IORequest *)tr, 0) != 0)
        return 1;
    TimerBase = tr->tr_node.io_Device;
    freq = ReadEClock(&e0);        /* returns the frequency */
    ReadEClock(&e0);

    for (i = 0; i < n; i++) {
        uint64_t d;
        tr->tr_node.io_Command = TR_ADDREQUEST;
        tr->tr_time.tv_secs = 0;
        tr->tr_time.tv_micro = 1000;
        SendIO((struct IORequest *)tr);
        WaitPort(p);
        (void)GetMsg(p);
        if (!CheckIO((struct IORequest *)tr))
            AbortIO((struct IORequest *)tr);
        WaitIO((struct IORequest *)tr);
        ReadEClock(&e1);
        d = eclk_us(&e1, freq) - eclk_us(&e0, freq);
        e0 = e1;
        if (d < min)
            min = d;
        if (d > max)
            max = d;
        sum += d;
    }

    f = fopen("RAM:TR.RES", "w");
    if (f) {
        fprintf(f, "n=%d min=%llu mean=%llu max=%llu us\n", n,
            (unsigned long long)min, (unsigned long long)(sum / (uint64_t)n),
            (unsigned long long)max);
        fclose(f);
    }
    CloseDevice((struct IORequest *)tr);
    DeleteIORequest(tr);
    DeleteMsgPort(p);
    return 0;
}
