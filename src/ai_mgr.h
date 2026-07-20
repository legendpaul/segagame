/*
 * ai_mgr.h - CPU opponent decision-making: throw timing/aim and catch
 * success chance. Kept separate from scene_match so difficulty can be
 * tuned in one place.
 */
#ifndef _AI_MGR_H_
#define _AI_MGR_H_

#include "genesis.h"

u16  ai_pickThrowDelay(void);
s16  ai_pickTargetX(s16 playerX);
bool ai_willCatch(void);

#endif /* _AI_MGR_H_ */
