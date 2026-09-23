/* audio_ahi_play.c — AROS low-level AHI backend glue, playback step (WBS 2.7).
 * AuPlay: render ao's song via AuRenderToFile to a RAM: temp, open ahi.device
 * unit 0, and stream the PCM with blocking CMD_WRITE. Whole-song render then
 * stream (streaming-with-render comes later). Self-contained: opens/closes
 * its own device handle, deletes the temp file — independent of any
 * negotiated session. AROS-only; ABI-portable C (v1 + v11 headers).
 */
#ifndef __AROS__
#error "audio_ahi_play.c is AROS-only"
#endif

#include <exec/types.h>
#include <exec/io.h>
#include <stdint.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "audio_io/audio_ahi.h"

extern struct DosLibrary *DOSBase;

/* AuRenderToFile lives in audio.c (shared); declared here to avoid
 * pulling audio.h's host shims into anything (they're harmless, just
 * unneeded). */
extern int AuRenderToFile(struct AudioObject *ao, const char *path,
                          uint32_t ms);

#define AUPLAY_TMP "RAM:auplay.wav"
#define AUPLAY_CHUNK 16384u

int AuPlay(struct AudioObject *ao) {
    struct MsgPort *port;
    struct AHIRequest *req;
    BPTR fh;
    UBYTE buf[AUPLAY_CHUNK];
    LONG rd;
    ULONG total = 0u;
    if (!ao)
        return 2;
    if (DOSBase)
        Printf((STRPTR)"RI_AUPLAY render...\n");
    if (AuRenderToFile(ao, AUPLAY_TMP, 0u) != 0) {
        if (DOSBase)
            Printf((STRPTR)"RI_AUPLAY render FAILED\n");
        return 10;
    }
    if (DOSBase)
        Printf((STRPTR)"RI_AUPLAY rendered ok\n");
    port = CreateMsgPort();
    if (!port)
        return 3;
    req = (struct AHIRequest *)CreateIORequest(port, sizeof *req);
    if (!req) {
        DeleteMsgPort(port);
        return 4;
    }
    if (OpenDevice((STRPTR)"ahi.device", 0, (struct IORequest *)req, 0)
        != 0) {
        if (DOSBase)
            Printf((STRPTR)"RI_AUPLAY open unit 0 FAILED ioerr=%ld\n",
                   (LONG)IoErr());
        DeleteIORequest((struct IORequest *)req);
        DeleteMsgPort(port);
        return 5;
    }
    fh = Open((STRPTR)AUPLAY_TMP, MODE_OLDFILE);
    if (!fh) {
        CloseDevice((struct IORequest *)req);
        DeleteIORequest((struct IORequest *)req);
        DeleteMsgPort(port);
        return 6;
    }
    if (Seek(fh, 44, OFFSET_BEGINNING) < 0) {
        Close(fh);
        CloseDevice((struct IORequest *)req);
        DeleteIORequest((struct IORequest *)req);
        DeleteMsgPort(port);
        return 7;
    }
    for (;;) {
        rd = Read(fh, buf, AUPLAY_CHUNK);
        if (rd <= 0)
            break;
        req->ahir_Std.io_Command = CMD_WRITE;
        req->ahir_Std.io_Data = (APTR)buf;
        req->ahir_Std.io_Length = rd;
        req->ahir_Version = 4;
        req->ahir_Type = AHIST_M16S; /* mono 16-bit (core renders mono) */
        req->ahir_Frequency = 48000UL;
        req->ahir_Volume = 0x10000UL;
        req->ahir_Position = 0x8000UL;
        DoIO((struct IORequest *)req);
        if (req->ahir_Std.io_Error != 0)
            break;
        total += (ULONG)rd;
    }
    Close(fh);
    CloseDevice((struct IORequest *)req);
    DeleteIORequest((struct IORequest *)req);
    DeleteMsgPort(port);
    DeleteFile((STRPTR)AUPLAY_TMP);
    if (DOSBase)
        Printf((STRPTR)"RI_AUPLAY played %lu bytes rc=0\n", total);
    return 0;
}