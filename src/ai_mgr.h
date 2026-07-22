/*
 * ai_mgr.h - CPU opponent decision-making: throw timing, aim and lane
 * selection. Kept separate so difficulty can be tuned in one place.
 */
#ifndef _AI_MGR_H_
#define _AI_MGR_H_

#include "genesis.h"

u16  ai_pickThrowDelay(void);
s16  ai_pickTargetX(s16 playerX);
/* Picks which of "count" in-play teammates/opponents an action applies
 * to (a thrower choosing who to aim at, or which in-play slot fills a
 * returning-player pick) - returns 0..count-1. */
u8   ai_pickSlot(u8 count);

#endif /* _AI_MGR_H_ */
