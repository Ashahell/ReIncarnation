/* tools/inspect — RBNM/RBNG validator/dumper (Task 9, gate G9).
 * S909-subset only (full formats + fuzz are Task 13 — NOT here).
 *
 * usage:
 *   inspect --rbnm FILE       validate pack: chunk lengths, S909/MANF/
 *                             SMPL presence + cross-refs, manifest
 *                             completeness. Prints "RBNM OK ..." or
 *                             "RBNM INVALID: <reason>".
 *   inspect --manifest FILE   validate a MANIFEST.txt alone (same line
 *                             rules). Prints "MANIFEST OK ..." or
 *                             "MANIFEST INVALID: <reason>".
 * exit: 0 valid, 1 invalid (with reason), 2 usage/IO error.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "project/rbnm.h"

static int read_text(const char *path, char *dst, uint32_t cap) {
    FILE *f = fopen(path, "rb");
    uint32_t n = 0;
    int ch;
    if (!f) {
        printf("inspect: cannot open %s\n", path);
        return 2;
    }
    while ((ch = fgetc(f)) != EOF) {
        if (n + 1u >= cap) {
            printf("inspect: file too large %s\n", path);
            fclose(f);
            return 2;
        }
        dst[n++] = (char)ch;
    }
    fclose(f);
    dst[n] = '\0';
    return 0;
}

int main(int argc, char **argv) {
    static char text[262144];
    char err[192];
    int rc;
    if (argc != 3 || (strcmp(argv[1], "--rbnm") != 0 &&
        strcmp(argv[1], "--manifest") != 0)) {
        printf("usage: inspect --rbnm FILE | --manifest FILE\n");
        return 2;
    }
    if (strcmp(argv[1], "--rbnm") == 0) {
        rc = rbnm_validate_file(argv[2], err, sizeof err);
        if (rc == 0) {
            printf("RBNM OK: %s\n", argv[2]);
            return 0;
        }
        printf("RBNM INVALID: %s: %s\n", argv[2], err);
        return 1;
    }
    rc = read_text(argv[2], text, sizeof text);
    if (rc != 0)
        return 2;
    rc = rbnm_validate_manifest_text(text, err, sizeof err);
    if (rc == 0) {
        printf("MANIFEST OK: %s\n", argv[2]);
        return 0;
    }
    printf("MANIFEST INVALID: %s: %s\n", argv[2], err);
    return 1;
}
