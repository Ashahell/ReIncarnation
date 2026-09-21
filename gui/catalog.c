/* catalog.c — locale string lookup (Task 14, gate G14).
 * No allocation, no libm, no platform includes: host + AROS clean.
 * Tables mirror locale/ReIncarnation.cd + locale/en.ct + locale/de.ct.
 */
#include "gui/catalog.h"
#include <string.h>

struct RICatEntry {
    const char *msgid;
    const char *en;
    const char *de; /* NULL = same as EN (untranslated) */
};

static const struct RICatEntry RI_CATALOG[] = {
    { "MSG_PLAY", "Play", "Abspielen" },
    { "MSG_STOP", "Stop", "Stopp" },
    { "MSG_TEMPO", "Tempo", NULL },
    { "MSG_MIXER", "Mixer", "Mischpult" },
    { "MSG_PATTERN", "Pattern", NULL },
    { "MSG_EXPORT", "Export WAV", "WAV exportieren" },
};

static int streq(const char *a, const char *b) {
    if (!a || !b)
        return 0;
    return strcmp(a, b) == 0;
}

const char *ri_catalog_get(const char *locale, const char *msgid) {
    unsigned int i;
    int want_de;
    if (!msgid)
        return "";
    want_de = streq(locale, RI_LOCALE_DE);
    for (i = 0; i < (unsigned int)(sizeof RI_CATALOG / sizeof RI_CATALOG[0]); i++) {
        if (streq(RI_CATALOG[i].msgid, msgid)) {
            if (want_de && RI_CATALOG[i].de)
                return RI_CATALOG[i].de;
            return RI_CATALOG[i].en;
        }
    }
    return msgid;
}
