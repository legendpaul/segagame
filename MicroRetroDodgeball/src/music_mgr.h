/*
 * music_mgr.h - Original scene-aware YM2612 title/menu score. Gameplay and
 * result scenes deliberately yield the mix to PCM stadium ambience.
 */
#ifndef _MUSIC_MGR_H_
#define _MUSIC_MGR_H_

#include "genesis.h"

void music_mgr_init(void);    /* call once at boot */
void music_mgr_update(void);  /* call once per frame */
void music_mgr_setMenuContext(bool titleScreen);

#endif /* _MUSIC_MGR_H_ */
