/* ri_pal_image.h — image decode (portability plan T7, §3.7).
 * C99, includes only <stdint.h>.
 * AROS: datatypes (size from PDTA_BitMapHeader). Host/Win: portable PNG
 * decoder (owner decision §8: vetted public-domain vs clean-room).
 * Skin blitting becomes RI_D_IMAGE in the display list; per-backend alpha.
 */
#ifndef RI_PAL_IMAGE_H
#define RI_PAL_IMAGE_H
#include <stdint.h>

int ri_pal_image_decode(const void *file, uint32_t len, uint32_t **rgba,
    uint32_t *w, uint32_t *h);
void ri_pal_image_free(uint32_t *rgba);

#endif
