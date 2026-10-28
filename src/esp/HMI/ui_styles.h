#ifndef UI_STYLES_H
#define UI_STYLES_H

#include <lvgl.h>

extern lv_style_t style_screen_bg;
extern lv_style_t style_top_bar;
extern lv_style_t style_card;
extern lv_style_t style_card_dark;
extern lv_style_t style_btn_primary;
extern lv_style_t style_btn_danger;
extern lv_style_t style_btn_success;
extern lv_style_t style_btn_nav;
extern lv_style_t style_text_title;
extern lv_style_t style_text_value;
extern lv_style_t style_text_label;
extern lv_style_t style_text_small;
extern lv_style_t style_indicator_on;
extern lv_style_t style_indicator_off;

void initStyles();

lv_obj_t *createCard(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);
lv_obj_t *createButton(lv_obj_t *parent, const char *text, lv_coord_t w,
                       lv_coord_t h, lv_style_t *style);
lv_obj_t *createTopBar(lv_obj_t *parent, const char *title, bool showBack);
lv_obj_t *createDivider(lv_obj_t *parent);
lv_obj_t *createStatusDot(lv_obj_t *parent, lv_coord_t size);
void setDotOn(lv_obj_t *dot, bool on, lv_color_t color);

/* Colour for a LinkRoastState value. */
lv_color_t getStateColor(int roastState);

#endif  // UI_STYLES_H
