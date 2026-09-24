/* gui/panels.h — RIPanelDesc tables (Task 12, gate G12).
 * Pure C (no MUI): host-compiled and host-queried; the AROS MCC shells
 * and app/main.c build real objects from these rows. Control IDs use
 * the spec §13 stable intent blocks: 303A 0x0300, 303B 0x0310, 808
 * 0x0400, 909 0x0900, FX 0x0A00, mixer/transport 0x0B00 (0x08xx
 * reserved). TC-2.9.5 generality proof: ri_panel_dummy() resolves
 * through the same lookup with zero special-casing.
 */
#ifndef RI_PANELS_H
#define RI_PANELS_H

#define RI_CTL_303A_BASE 0x0300u
#define RI_CTL_303B_BASE 0x0310u
#define RI_CTL_808_BASE 0x0400u
#define RI_CTL_909_BASE 0x0900u
#define RI_CTL_FX_BASE 0x0A00u
#define RI_CTL_MIX_BASE 0x0B00u

struct RIPanelControl {
    unsigned int ctl_id;
    const char *name;
    unsigned char def_value;
    unsigned char min_v;
    unsigned char max_v;
};

struct RIPanelDesc {
    const char *name;
    unsigned int device; /* 0..3 classic (303A, 303B, 808, 909), 4 mix, 5 transport */
    const struct RIPanelControl *ctls;
    unsigned int nctls;
};

unsigned int ri_panel_count(void);
const struct RIPanelDesc *ri_panel_get(unsigned int i); /* NULL out of range */
const struct RIPanelControl *ri_panel_find_ctl(const struct RIPanelDesc *p,
                                               unsigned int ctl_id);
const struct RIPanelDesc *ri_panel_dummy(void);
/* Right-click → default: the control's neutral value, or -1 when the
 * panel is NULL or carries no such control (fail-closed; the MCC
 * shell ignores -1). TC-2.9.2. */
int ri_panel_default_ctl(const struct RIPanelDesc *p, unsigned int ctl_id);

/* Value readout formatting (proof-vehicle text row): 0..127 as
 * decimal into buf (caller provides 4 bytes); out-of-range
 * fail-closed to "---". NULL buf ignored. */
void ri_ctl_format_value(char *buf, int v);

/* Undo-commit counter formatting (arbitrary counts): decimal into
 * buf (caller provides 6 bytes, up to 65535); saturates above.
 * NULL buf ignored. */
void ri_ctl_format_count(char *buf, unsigned long n);

/* First-panel knob rects (TC-2.9.1 code side): panel-909-geometry
 * doc centers, scaled to a zoom level (0/1/2). Index 0..3 =
 * tune/level/decay/flamres; anything else (or unknown zoom)
 * returns all zeros. */
struct RIPanelRect {
    int x; /* center */
    int y; /* center */
    int w; /* width */
    int h; /* height */
};
struct RIPanelRect ri_panel909_knob_rect(unsigned int index, int zoom);

/* First-panel composition constants (TC-2.9.1 code side): proof
 * window size + background, shared by the proof vehicle and the
 * future MCC panel so both paint the same canvas. */
#define RI_PANEL909_W 296u
#define RI_PANEL909_H 96u
#define RI_PANEL909_BG 0xdcdcd6u

#endif
