/* Arcade-style MICRO RETRO DODGEBALL title artwork. */
#ifndef _TITLE_DATA_H_
#define _TITLE_DATA_H_

#include "genesis.h"
#include "flag_data.h"

#define TILE_TITLE_BASE  (TILE_FLAG_BASE + FLAG_TILE_COUNT)

void title_data_init(void);
void title_data_draw(void);

#endif /* _TITLE_DATA_H_ */
