/* gui/zoomfit.h — pure Fit-mode zoom choice (S5, 2026-09-29).
 * Picks the largest content zoom (2/1/0) whose window fits the screen.
 * Host-testable: no MUI here; the app feeds screen/chrome sizes.
 * Content sizes come from gui/panelgeo (canvas Q) + gui/tabpages (page
 * membership); rack furniture uses the app's layout numbers (ledgered).
 */
#ifndef RI_ZOOMFIT_H
#define RI_ZOOMFIT_H

/* Fit mode (re-fit at startup); explicit zooms are 0 = 1x, 1 = 1.5x, 2 = 2x. */
#define RI_ZOOMFIT_FIT (-1)

/* Largest zoom 2/1/0 with content+chrome inside scr_w/scr_h; 0 fallback
 * (fail-closed, e.g. 800x600: nothing fits, stay small). Non-positive
 * inputs fail closed to 0. */
int ri_zoom_fit(int scr_w, int scr_h, int chrome_w, int chrome_h);
/* Content window (canvases + furniture + root gaps, no window chrome) at
 * zoom 0..2. 0 ok, 2 on bad zoom/NULL. Transport stays compact (S5). */
int ri_zoomfit_content(int zoom, int *w, int *h);
/* Guard (owner breakage 2026-09-29: 1.5x on 1366x768 drops rail/tabs):
 * explicit want clamped to what fits; FIT re-fits. Unknown screen
 * (non-positive) keeps a valid want, 0 otherwise. */
int ri_zoom_clamp(int scr_w, int scr_h, int chrome_w, int chrome_h, int want);
/* Persisted choice: "fit"/"0"/"1"/"2" -> -1/0/1/2, anything else -> -1.
 * Format writes the word (no NUL counted); 0 on bad zoom/buffer. */
int ri_zoom_parse(const char *s, unsigned n);
int ri_zoom_format(int zoom, char *out, unsigned cap);
/* Levi canvas zoom (fidelity plan P1): the largest zoom >= the app zoom
 * whose window (Levi page at that zoom, all else at the app zoom) fits;
 * the app zoom when nothing larger fits or the screen is unknown. */
int ri_zoom_levi(int zoom, int scr_w, int scr_h, int chrome_w, int chrome_h);

#endif
