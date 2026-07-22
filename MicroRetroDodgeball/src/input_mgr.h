/*
 * input_mgr.h - Joypad polling + edge detection for player 1.
 */
#ifndef _INPUT_MGR_H_
#define _INPUT_MGR_H_

#include "genesis.h"

void input_mgr_update(void);
bool input_held(u16 button);
bool input_pressed(u16 button);   /* true only on the frame it was first pressed */

#endif /* _INPUT_MGR_H_ */
