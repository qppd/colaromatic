#include "ui_styles.h"

#include "LinkFrames.h"
#include "screen_manager.h"
#include "ui_theme.h"

lv_style_t style_screen_bg;
lv_style_t style_top_bar;
lv_style_t style_card;
lv_style_t style_card_dark;
lv_style_t style_btn_primary;
lv_style_t style_btn_danger;
lv_style_t style_btn_success;
lv_style_t style_btn_nav;
lv_style_t style_text_title;
lv_style_t style_text_value;
lv_style_t style_text_label;
lv_style_t style_text_small;
lv_style_t style_indicator_on;
lv_style_t style_indicator_off;

void initStyles() {
  lv_style_init(&style_screen_bg);
  lv_style_set_bg_color(&style_screen_bg, COLOR_BG);
  lv_style_set_bg_opa(&style_screen_bg, LV_OPA_COVER);
  lv_style_set_pad_all(&style_screen_bg, 0);
  lv_style_set_border_width(&style_screen_bg, 0);
  lv_style_set_radius(&style_screen_bg, 0);

  lv_style_init(&style_top_bar);
  lv_style_set_bg_color(&style_top_bar, COLOR_BG_TOP_BAR);
  lv_style_set_bg_opa(&style_top_bar, LV_OPA_COVER);
  lv_style_set_pad_left(&style_top_bar, SIDE_PADDING);
  lv_style_set_pad_right(&style_top_bar, SIDE_PADDING);
  lv_style_set_pad_ver(&style_top_bar, 5);
  lv_style_set_border_side(&style_top_bar, LV_BORDER_SIDE_BOTTOM);
  lv_style_set_border_width(&style_top_bar, 1);
  lv_style_set_border_color(&style_top_bar, COLOR_BORDER);
  lv_style_set_radius(&style_top_bar, 0);

  lv_style_init(&style_card);
  lv_style_set_bg_color(&style_card, COLOR_BG_CARD);
  lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
  lv_style_set_radius(&style_card, 12);
  lv_style_set_pad_all(&style_card, 12);
  lv_style_set_border_width(&style_card, 1);
  lv_style_set_border_color(&style_card, COLOR_BORDER);

  lv_style_init(&style_card_dark);
  lv_style_set_bg_color(&style_card_dark, COLOR_BG_CARD_DARK);
  lv_style_set_bg_opa(&style_card_dark, LV_OPA_COVER);
  lv_style_set_radius(&style_card_dark, 12);
  lv_style_set_pad_all(&style_card_dark, 12);
  lv_style_set_border_width(&style_card_dark, 1);
  lv_style_set_border_color(&style_card_dark, COLOR_BORDER);

  lv_style_init(&style_btn_primary);
  lv_style_set_bg_color(&style_btn_primary, COLOR_ACCENT);
  lv_style_set_bg_opa(&style_btn_primary, LV_OPA_COVER);
  lv_style_set_radius(&style_btn_primary, 8);
  lv_style_set_text_color(&style_btn_primary, COLOR_TEXT_PRIMARY);
  lv_style_set_text_font(&style_btn_primary, FONT_MEDIUM);
  lv_style_set_pad_hor(&style_btn_primary, 16);
  lv_style_set_pad_ver(&style_btn_primary, 8);
  lv_style_set_shadow_color(&style_btn_primary, COLOR_ACCENT);
  lv_style_set_shadow_width(&style_btn_primary, 8);
  lv_style_set_shadow_opa(&style_btn_primary, LV_OPA_30);

  lv_style_init(&style_btn_danger);
  lv_style_set_bg_color(&style_btn_danger, COLOR_DANGER);
  lv_style_set_bg_opa(&style_btn_danger, LV_OPA_COVER);
  lv_style_set_radius(&style_btn_danger, 8);
  lv_style_set_text_color(&style_btn_danger, COLOR_TEXT_PRIMARY);
  lv_style_set_text_font(&style_btn_danger, FONT_MEDIUM);
  lv_style_set_pad_hor(&style_btn_danger, 16);
  lv_style_set_pad_ver(&style_btn_danger, 8);

  lv_style_init(&style_btn_success);
  lv_style_set_bg_color(&style_btn_success, COLOR_SUCCESS);
  lv_style_set_bg_opa(&style_btn_success, LV_OPA_COVER);
  lv_style_set_radius(&style_btn_success, 8);
  lv_style_set_text_color(&style_btn_success, COLOR_TEXT_PRIMARY);
  lv_style_set_text_font(&style_btn_success, FONT_MEDIUM);
  lv_style_set_pad_hor(&style_btn_success, 16);
  lv_style_set_pad_ver(&style_btn_success, 8);

  lv_style_init(&style_btn_nav);
  lv_style_set_bg_color(&style_btn_nav, COLOR_BG_BUTTON);
  lv_style_set_bg_opa(&style_btn_nav, LV_OPA_COVER);
  lv_style_set_radius(&style_btn_nav, 8);
  lv_style_set_text_color(&style_btn_nav, COLOR_TEXT_PRIMARY);
  lv_style_set_text_font(&style_btn_nav, FONT_NORMAL);
  lv_style_set_pad_hor(&style_btn_nav, 12);
  lv_style_set_pad_ver(&style_btn_nav, 6);

  lv_style_init(&style_text_title);
  lv_style_set_text_color(&style_text_title, COLOR_TEXT_PRIMARY);
  lv_style_set_text_font(&style_text_title, FONT_LARGE);

  lv_style_init(&style_text_value);
  lv_style_set_text_color(&style_text_value, COLOR_TEXT_PRIMARY);
  lv_style_set_text_font(&style_text_value, FONT_XL);

  lv_style_init(&style_text_label);
  lv_style_set_text_color(&style_text_label, COLOR_TEXT_SECONDARY);
  lv_style_set_text_font(&style_text_label, FONT_SMALL);

  lv_style_init(&style_text_small);
  lv_style_set_text_color(&style_text_small, COLOR_TEXT_SECONDARY);
  lv_style_set_text_font(&style_text_small, FONT_SMALL);

  lv_style_init(&style_indicator_on);
  lv_style_set_bg_color(&style_indicator_on, COLOR_SUCCESS);
  lv_style_set_bg_opa(&style_indicator_on, LV_OPA_COVER);
  lv_style_set_radius(&style_indicator_on, LV_RADIUS_CIRCLE);

  lv_style_init(&style_indicator_off);
  lv_style_set_bg_color(&style_indicator_off, COLOR_BG_BUTTON);
  lv_style_set_bg_opa(&style_indicator_off, LV_OPA_COVER);
  lv_style_set_radius(&style_indicator_off, LV_RADIUS_CIRCLE);
}

lv_obj_t *createCard(lv_obj_t *parent, lv_coord_t w, lv_coord_t h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_add_style(card, &style_card, 0);
  lv_obj_set_size(card, w, h);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
  return card;
}

lv_obj_t *createButton(lv_obj_t *parent, const char *text, lv_coord_t w,
                       lv_coord_t h, lv_style_t *style) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_add_style(btn, style, 0);
  lv_obj_set_size(btn, w, h);

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_center(label);

  return btn;
}

lv_obj_t *createTopBar(lv_obj_t *parent, const char *title, bool showBack) {
  lv_obj_t *bar = lv_obj_create(parent);
  lv_obj_add_style(bar, &style_top_bar, 0);
  lv_obj_set_size(bar, SCREEN_WIDTH, TOP_BAR_HEIGHT);
  lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bar, 10, 0);

  if (showBack) {
    lv_obj_t *backBtn = lv_btn_create(bar);
    lv_obj_add_style(backBtn, &style_btn_nav, 0);
    lv_obj_set_size(backBtn, BTN_MIN_SIZE, 36);
    lv_obj_t *backLabel = lv_label_create(backBtn);
    lv_label_set_text(backLabel, LV_SYMBOL_LEFT);
    lv_obj_center(backLabel);
    lv_obj_add_event_cb(backBtn, backBtnCallback, LV_EVENT_CLICKED, NULL);
  }

  lv_obj_t *titleLabel = lv_label_create(bar);
  lv_obj_add_style(titleLabel, &style_text_title, 0);
  lv_label_set_text(titleLabel, title);

  lv_obj_t *spacer = lv_obj_create(bar);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(spacer, 0, 0);
  lv_obj_set_style_pad_all(spacer, 0, 0);
  lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_height(spacer, 1);

  return bar;
}

lv_obj_t *createDivider(lv_obj_t *parent) {
  lv_obj_t *sep = lv_obj_create(parent);
  lv_obj_set_size(sep, LV_PCT(100), 1);
  lv_obj_set_style_bg_color(sep, COLOR_BORDER, 0);
  lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_set_scrollbar_mode(sep, LV_SCROLLBAR_MODE_OFF);
  return sep;
}

lv_obj_t *createStatusDot(lv_obj_t *parent, lv_coord_t size) {
  lv_obj_t *dot = lv_obj_create(parent);
  lv_obj_set_size(dot, size, size);
  lv_obj_add_style(dot, &style_indicator_off, 0);
  lv_obj_set_scrollbar_mode(dot, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_pad_all(dot, 0, 0);
  return dot;
}

void setDotOn(lv_obj_t *dot, bool on, lv_color_t color) {
  if (!lv_obj_is_valid(dot)) return;
  lv_obj_set_style_bg_color(dot, on ? color : COLOR_BG_BUTTON, 0);
  if (on) {
    lv_obj_set_style_shadow_color(dot, color, 0);
    lv_obj_set_style_shadow_width(dot, 8, 0);
    lv_obj_set_style_shadow_opa(dot, LV_OPA_50, 0);
  } else {
    lv_obj_set_style_shadow_width(dot, 0, 0);
  }
}

lv_color_t getStateColor(int roastState) {
  switch (roastState) {
    case ROAST_IDLE: return COLOR_IDLE;
    case ROAST_RUNNING: return COLOR_HEATING;
    case ROAST_HOLD: return COLOR_WARNING;
    case ROAST_COOLING: return COLOR_IDLE;
    case ROAST_DONE: return COLOR_SUCCESS;
    default: return COLOR_TEXT_SECONDARY;
  }
}
