/* art_909.c — 909 background + legends (portability plan T2). */
#include "gui/draw/art.h"

#include <string.h>
#include "gui/panelgeo.h"

static const struct { int x0, x1; const char *t; } ART_909_BAR[] = {
    { 22, 98, "AC" }, { 108, 272, "BASS DRUM" }, { 276, 440, "SNARE DRUM" }, { 444, 608, "LOW TOM" },
    { 612, 776, "MID TOM" }, { 780, 944, "HI TOM" }, { 948, 1028, "RIM" }, { 1032, 1112, "CLAP" },
    { 1116, 1280, "HI HAT" }, { 1284, 1448, "CYMBAL" }
};

static const char *const ART_909_OPT[12] = {
    "AC", "BASS DRUM", "SNARE DRUM", "LOW TOM", "MID TOM", "HI TOM", "RS", "CP", "CH", "OH", "CC", "RC"
};

const char *ri_art_909_opt(uint32_t opt) {
    return opt < 12 ? ART_909_OPT[opt] : "?";
}

const char *ri_art_legend_909(const char *leg) {
    if (!leg)
        return leg;
    return !strcmp(leg, "Attack") ? "ATT" : !strcmp(leg, "Decay") ? "DEC" : !strcmp(leg, "Snappy") ? "SNAP" : leg;
}

const char *ri_art_legend_fx(const char *leg) {
    if (!leg)
        return leg;
    return !strcmp(leg, "Decay") ? "DEC" : !strcmp(leg, "Threshold") ? "THRES" : !strcmp(leg, "Loop Start") ? "START"
         : !strcmp(leg, "Loop Length") ? "LENGTH" : leg;
}

void ri_art_bg_909(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z) {
    uint32_t i;
    char n[3];
#define PX(q) ri_geo_px((q), z)
    ri_art_rect(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_909_PANEL);
    for (i = 0; i < sizeof(ART_909_BAR) / sizeof(ART_909_BAR[0]); i++) {
        int x0 = ART_909_BAR[i].x0, x1 = ART_909_BAR[i].x1;
        ri_art_rect(dl, ox + PX(x0), oy + PX(26), ox + PX(x1), oy + PX(56), C_909_BAR);
        ri_art_text_c(dl, ox + PX((x0 + x1) / 2), oy + PX(41), ART_909_BAR[i].t, C_909_ORANGE);
    }
    for (i = 0; i <= 8; i++)                   /* group dividers, as on the TR-909 panel */
        ri_art_line(dl, ox + PX(106 + 168 * (int)i), oy + PX(56), ox + PX(106 + 168 * (int)i), oy + PX(296), C_909_BAR);
    ri_art_line(dl, ox + PX(1030), oy + PX(56), ox + PX(1030), oy + PX(296), C_909_BAR);
    ri_art_line(dl, ox + PX(106), oy + PX(296), ox + PX(1448), oy + PX(296), C_909_BAR);
    for (i = 0; i < 16; i++) {                 /* step numbers under the keys (p. 151) */
        n[0] = (char)(i >= 9 ? '1' : '0' + (i + 1));
        n[1] = (char)(i >= 9 ? '0' + (i + 1 - 10) : 0);
        n[2] = 0;
        ri_art_rect(dl, ox + PX(148 + 84 * (int)i - 40), oy + PX(418), ox + PX(148 + 84 * (int)i + 40), oy + PX(446),
            C_909_BAR);
        ri_art_text_c(dl, ox + PX(148 + 84 * (int)i), oy + PX(432), n, C_CREAM);
    }
#undef PX
}
