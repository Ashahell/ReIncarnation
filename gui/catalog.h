/* catalog.h — locale string lookup (Task 14, gate G14).
 * Pure/host-tested (no Amiga includes): msgid → localized string with
 * EN fallback. The .cd/.ct sources live in locale/ (CatComp); these
 * tables mirror them 1:1 — the audit greps the DE proof string in
 * BOTH the .ct source and this table so they cannot drift apart.
 * Unknown locale → EN. Unknown msgid → the msgid itself (fail-open
 * display, never NULL).
 */
#ifndef RI_CATALOG_H
#define RI_CATALOG_H

#define RI_LOCALE_EN "EN"
#define RI_LOCALE_DE "DE"

/* Proof string (Task 14): the one translated string the GUI
 * acceptance re-run renders. German for "Play". */
#define RI_MSG_PLAY "MSG_PLAY"
#define RI_DE_PLAY "Abspielen"

const char *ri_catalog_get(const char *locale, const char *msgid);
#endif
