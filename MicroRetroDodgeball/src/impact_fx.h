#ifndef _IMPACT_FX_H_
#define _IMPACT_FX_H_

#include "genesis.h"

/* One highly readable contact burst is shared by all live balls. A new impact
 * replaces the old one, which is preferable to spending extra sprite slots in
 * the 10-player Eliminator scene. */
void impact_fx_init(void);
void impact_fx_update(void);
void impact_fx_trigger(s16 x, s16 y, s16 sortX, s16 sortY,
                       bool highPriority, bool playerContact);
bool impact_fx_active(void);
s16 impact_fx_sortX(void);
s16 impact_fx_sortY(void);
void impact_fx_setSpriteSlot(u8 slot);
void impact_fx_draw(s16 worldOffsetY);

#endif
