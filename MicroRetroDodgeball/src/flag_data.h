/* National flag tiles and the 5x2 team-selection grid. */
#ifndef _FLAG_DATA_H_
#define _FLAG_DATA_H_

#include "genesis.h"
#include "logo_data.h"

#define TILE_FLAG_BASE   (TILE_LOGO_BASE + LOGO_TILE_COUNT)
#define FLAG_TILE_COUNT  105

void flag_data_init(void);
void flag_data_fill_panel(u16 x, u16 y, u16 w, u16 h);
void flag_data_draw_grid(u8 selected);
void flag_data_draw_selector(u8 selected, u8 playerNumber);
void flag_data_draw_small(u8 teamIndex, u16 x, u16 y, u8 palette);
void flag_data_draw_large(u8 teamIndex, u16 x, u16 y, u8 palette);
void flag_data_draw_matchup(u8 teamAIndex, u8 teamBIndex);

#endif /* _FLAG_DATA_H_ */
