#include "player.h"
#include "input_mgr.h"
#include "sprites_data.h"
#include "game_state.h"

void player_init(Player *p, s16 startX, s16 y, u8 spriteSlot, u8 pal, u8 lives)
{
    p->x = startX;
    p->y = y;
    p->lives = lives;
    p->spriteSlot = spriteSlot;
    p->pal = pal;
}

void player_moveHuman(Player *p)
{
    if (input_held(BUTTON_LEFT))  p->x -= PLAYER_SPEED;
    if (input_held(BUTTON_RIGHT)) p->x += PLAYER_SPEED;

    if (p->x < COURT_LEFT_X)  p->x = COURT_LEFT_X;
    if (p->x > COURT_RIGHT_X) p->x = COURT_RIGHT_X;
}

void player_draw(Player *p)
{
    /* Hardware sprites form a linked list starting at slot 0; a sprite
     * whose slot isn't reachable via some other sprite's "link" is never
     * rendered. We keep a fixed chain: slot N links to slot N+1. */
    VDP_setSpriteFull(p->spriteSlot, p->x, p->y, SPRITE_SIZE(1, 2),
                       TILE_ATTR_FULL(p->pal, 0, FALSE, FALSE, TILE_PLAYER_TOP),
                       p->spriteSlot + 1);
}
