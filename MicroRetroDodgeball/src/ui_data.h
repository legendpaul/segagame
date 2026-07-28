/* Shared authored UI lettering, score numerals and metallic panels. */
#ifndef _UI_DATA_H_
#define _UI_DATA_H_

#include "genesis.h"
#include "title_data.h"

#define TILE_UI_BASE  (TILE_TITLE_BASE + TITLE_TILE_COUNT)

#define UI_WHITE 0
#define UI_GOLD  1
#define UI_CYAN  2

void ui_data_init(void);
void ui_set_palette(u8 palette);
void ui_apply_palette(void);
void ui_draw_text(const char *text, u16 x, u16 y, u8 style);
void ui_draw_text_center(const char *text, u16 y, u8 style);
void ui_draw_big_text(const char *text, u16 x, u16 y, u8 style);
void ui_draw_big_center(const char *text, u16 y, u8 style);
void ui_draw_panel(u16 x, u16 y, u16 w, u16 h, bool gold);
void ui_clear_panel_interior(u16 x, u16 y, u16 w, u16 h);
/* Tournament-bracket join linking two paired rows in column x, with a stub
 * leaving the midpoint toward the next round. */
void ui_draw_bracket(u16 x, u16 yTop, u16 yBot, bool mirrored);
/* Horizontal connector run, w tiles wide. */
void ui_draw_hrule(u16 x, u16 y, u16 w);
void ui_draw_button(const char *label, u16 x, u16 y, u16 w);

/* Gameplay sprites sometimes need high priority to pass in front of the
 * centre board. Register the temporary HUD panel here so only sprites whose
 * pixel bounds touch it are lowered behind the UI. The permanent top score
 * strip is always included by ui_sprite_behind_panel(). */
void ui_set_sprite_panel_mask(bool enabled, s16 x, s16 y, u16 w, u16 h);
bool ui_sprite_behind_panel(s16 x, s16 y, u16 w, u16 h);

#endif /* _UI_DATA_H_ */
