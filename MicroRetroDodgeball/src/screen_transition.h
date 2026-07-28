#ifndef _SCREEN_TRANSITION_H_
#define _SCREEN_TRANSITION_H_

#include "genesis.h"

/* Short broadcast-style dip to black: quick exit, slightly softer reveal.
 * Scene drawing happens while black so VRAM changes never flash on screen. */
void screen_transition_fade_out(void);
void screen_transition_fade_in(void);

#endif /* _SCREEN_TRANSITION_H_ */
