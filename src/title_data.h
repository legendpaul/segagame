/* Arcade-style MICRO RETRO DODGEBALL title artwork. */
#ifndef _TITLE_DATA_H_
#define _TITLE_DATA_H_

#include "genesis.h"
#include "flag_data.h"

#define TILE_TITLE_BASE  (TILE_FLAG_BASE + FLAG_TILE_COUNT)
/* UI keeps its original static base after this reserve. The full title
 * illustration is a scene-local VRAM bank that deliberately overlays UI
 * tiles and is replaced before the selectors are drawn. */
#define TITLE_TILE_COUNT 68

void title_data_init(void);
void title_data_draw(void);
void title_data_set_prompt(bool visible);

#endif /* _TITLE_DATA_H_ */
