#include <genesis.h>

#ifndef _RES_AUDIO_H_
#define _RES_AUDIO_H_

// Music Resources (PCM4, 8-bit mono, 16 kHz)
extern const u8 MUSIC_TITLE_LOOP[876288];
extern const u8 MUSIC_MENU_LOOP[463872];
extern const u8 MUSIC_TOURNAMENT_LOOP[553216];
extern const u8 MUSIC_TOURNAMENT_WIN[384000];

// Stadium Crowd & SFX Resources (PCM4, 8-bit mono, 16 kHz)
extern const u8 CROWD_NORMAL_01[64000];
extern const u8 CROWD_CHANT[64000];
extern const u8 CROWD_NORMAL_02[64000];
extern const u8 CROWD_NORMAL_03[64000];
extern const u8 CROWD_GOAL[51200];
extern const u8 CROWD_MATCH_WIN[80128];
extern const u8 CROWD_TOURNAMENT_WIN[104192];
extern const u8 CROWD_WORLD_CUP_WIN[128000];
extern const u8 SFX_WHISTLE[12544];
extern const u8 SFX_MENU_SELECT[3328];
extern const u8 SFX_MENU_MOVE[2560];
extern const u8 SFX_MENU_UP[2560];
extern const u8 SFX_MENU_DOWN[2560];

// Game Physical SFX Resources (kept from existing project and compiled as PCM4)
extern const u8 SFX_THROW[2816];
extern const u8 SFX_PICKUP[1792];
extern const u8 SFX_HIT[4096];
extern const u8 SFX_BOUNCE[2048];

#endif // _RES_AUDIO_H_
