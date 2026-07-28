#include "impact_fx.h"
#include "sprites_data.h"
#include "ui_data.h"
#include "court_bg.h"

typedef struct {
    s16 x, y;
    s16 sortX, sortY;
    u16 timer, duration;
    u8 slot;
    bool highPriority;
    bool playerContact;
} ImpactFx;

static ImpactFx effect;

void impact_fx_init(void)
{
    effect.x = effect.y = -32;
    effect.sortX = effect.sortY = -32767;
    effect.timer = 0;
    effect.duration = 0;
    effect.slot = 0;
    effect.highPriority = FALSE;
    effect.playerContact = FALSE;
}

void impact_fx_trigger(s16 x, s16 y, s16 sortX, s16 sortY,
                       bool highPriority, bool playerContact)
{
    effect.x = x;
    effect.y = y;
    effect.sortX = sortX;
    effect.sortY = sortY;
    effect.duration = SYS_isPAL() ? 15 : 18; /* 30% of the original duration */
    effect.timer = effect.duration;
    effect.highPriority = highPriority;
    effect.playerContact = playerContact;
}

void impact_fx_update(void)
{
    if (effect.timer > 0) effect.timer--;
}

bool impact_fx_active(void) { return effect.timer > 0; }
s16 impact_fx_sortX(void) { return effect.sortX; }
s16 impact_fx_sortY(void) { return effect.sortY; }
void impact_fx_setSpriteSlot(u8 slot) { effect.slot = slot; }

void impact_fx_draw(s16 worldOffsetY)
{
    bool visible = effect.timer > 0;
    bool highPriority = effect.highPriority;
    /* Let the last third crackle out instead of disappearing on one frame. */
    if (visible && effect.timer < effect.duration / 3 &&
        ((effect.timer >> 2) & 1) == 0)
        visible = FALSE;
    if (ui_sprite_behind_panel(effect.x - 8,
                               effect.y - 8 + worldOffsetY, 16, 16) ||
        court_bg_spriteBehindRoof(effect.x - 8,
                                  effect.y - 8 + worldOffsetY, 16, 16))
        highPriority = FALSE;

    VDP_setSpriteFull(effect.slot,
                      visible ? effect.x - 8 : -32,
                      visible ? effect.y - 8 + worldOffsetY : -32,
                      SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL0, highPriority ? 1 : 0,
                                     FALSE, FALSE,
                                     effect.playerContact
                                         ? TILE_IMPACT_BURST
                                         : TILE_IMPACT_GREY),
                      (u8)(effect.slot + 1));
}
