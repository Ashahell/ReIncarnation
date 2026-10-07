/* BLITPROBE — screen write bandwidth probe (F2 baseline).
 *
 * Source verbatim from docs/evidence/gui/tab-switch/2026-10-07-b0-split.md
 * (advisor addendum 2026-10-07), plus the manual dos.library open/close
 * (startup.o leaves DOSBase NULL; first Printf through it faults —
 * found with MEMTYPE on riqemu1 2026-10-07, same pattern as
 * audio_io/probe_ahi.c). No other changes.
 */
#include <exec/types.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/timer.h>
struct Device *TimerBase;
static ULONG ef;
static UQUAD now(void) { struct EClockVal e; ReadEClock(&e); return ((UQUAD)e.ev_hi << 32) | e.ev_lo; }
static ULONG us(UQUAD a, UQUAD b) { return (ULONG)((b - a) * 1000000ULL / ef); }
int main(void) {
    struct timerequest tr;
    struct Window *w;
    struct BitMap *bm, *bm2;
    struct RastPort brp;
    int W = 1024, H = 512, i, N = 20, d;
    UQUAD t0, t1;
    ULONG u, mb;
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0);
    if (!DOSBase)
        return 20;
    if (OpenDevice((STRPTR)"timer.device", UNIT_ECLOCK, (struct IORequest *)&tr, 0)) return 20;
    TimerBase = tr.tr_node.io_Device;
    { struct EClockVal e; ef = ReadEClock(&e); }
    w = OpenWindowTags(NULL, WA_Left, 40, WA_Top, 40, WA_InnerWidth, W, WA_InnerHeight, H,
        WA_Title, (IPTR)"BLITPROBE", WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE, TAG_DONE);
    if (!w) { Printf("no window\n"); return 20; }
    d = GetBitMapAttr(w->RPort->BitMap, BMA_DEPTH);
    bm = AllocBitMap(W, H, d, BMF_MINPLANES, w->RPort->BitMap);
    bm2 = AllocBitMap(W, H, d, BMF_MINPLANES, w->RPort->BitMap);
    if (!bm || !bm2) { Printf("no bitmap\n"); CloseWindow(w); return 20; }
    InitRastPort(&brp); brp.BitMap = bm;
    SetAPen(&brp, 3); RectFill(&brp, 0, 0, W - 1, H - 1);
    mb = (ULONG)W * H * 4u;
    Printf("depth=%ld bytes/frame=%lu (at 4 B/px)\n", (LONG)d, mb);
    t0 = now(); for (i = 0; i < N; i++) BltBitMap(bm, 0, 0, bm2, 0, 0, W, H, 0xC0, 0xFF, NULL); t1 = now();
    u = us(t0, t1) / N; Printf("RAM->RAM  BltBitMap        %lu us/frame  %lu MB/s\n", u, u ? mb / u : 0);
    t0 = now(); for (i = 0; i < N; i++) BltBitMapRastPort(bm, 0, 0, w->RPort, w->BorderLeft, w->BorderTop, W, H, 0xC0); t1 = now();
    u = us(t0, t1) / N; Printf("RAM->WIN  BltBitMapRastPort %lu us/frame  %lu MB/s\n", u, u ? mb / u : 0);
    SetAPen(w->RPort, 2);
    t0 = now(); for (i = 0; i < N; i++) RectFill(w->RPort, w->BorderLeft, w->BorderTop, w->BorderLeft + W - 1, w->BorderTop + H - 1); t1 = now();
    u = us(t0, t1) / N; Printf("WIN       RectFill full    %lu us/frame  %lu MB/s\n", u, u ? mb / u : 0);
    t0 = now(); for (i = 0; i < N * 50; i++) RectFill(w->RPort, w->BorderLeft, w->BorderTop + (i % H), w->BorderLeft + W - 1, w->BorderTop + (i % H)); t1 = now();
    u = us(t0, t1); Printf("WIN       RectFill 1-row x%ld  %lu us total  %lu us/row\n", (LONG)(N * 50), u, u / (N * 50));
    FreeBitMap(bm); FreeBitMap(bm2); CloseWindow(w); CloseDevice((struct IORequest *)&tr);
    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
