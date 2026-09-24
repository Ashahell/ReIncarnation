/* gui/widgets/rlbl.h — RLbl static label class API (Module 2.9).
 *
 * AROS-ONLY. Tiny Area subclass: light panel fill + centered dark
 * text (the knobproof-proven calls). Exists because MUIC_Text
 * renders label rows inverted (black bg) for reasons no header
 * explains — this class owns every pixel it paints. Must NEVER
 * enter the host build (audit gates it).
 */
#ifndef RI_RLBL_H
#define RI_RLBL_H

#ifndef __AROS__
#error "rlbl.h is AROS-only: Zune custom class API, never in the host build"
#endif

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>

struct MUI_CustomClass;

/* Label text (STRPTR, copied at creation). */
#define MUIA_RLbl_Text (TAG_USER + 0x524Cu)

struct MUI_CustomClass *ri_rlbl_class(void);
void ri_rlbl_dispose_class(void);

/* Create one 80x14 label. NULL = class failed. */
APTR ri_rlbl_create(const char *text);

#endif
