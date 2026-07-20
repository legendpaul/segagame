/*
 * player.h - A court-side entity (human or CPU side), rendered as one
 * 8x16 hardware sprite (two stacked 8x8 tiles: TILE_PLAYER_TOP/BOTTOM).
 */
#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "genesis.h"

typedef struct {
    s16 x;
    s16 y;
    u8  lives;
    u8  spriteSlot;
    u8  pal;
} Player;

void player_init(Player *p, s16 startX, s16 y, u8 spriteSlot, u8 pal, u8 lives);
void player_moveHuman(Player *p);
void player_draw(Player *p);

#endif /* _PLAYER_H_ */
