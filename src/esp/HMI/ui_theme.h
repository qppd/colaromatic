/*
 * ui_theme.h — layout constants, palette and fonts for the ColAromatic panel.
 *
 * Same industrial dark scheme and 800x480 geometry as the reference
 * MultiDryerHMI sketch, retuned for a coffee roaster: the gauge covers the
 * roast band (0-250 degC) instead of a dryer's 0-120, and the alert colours
 * carry the machine's safety states.
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>

/* ---- geometry ---------------------------------------------------------- */
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

#define TOP_BAR_HEIGHT 50
#define BOTTOM_BAR_HEIGHT 60
#define CONTENT_HEIGHT (SCREEN_HEIGHT - TOP_BAR_HEIGHT - BOTTOM_BAR_HEIGHT)
#define SIDE_PADDING 15
#define WIDGET_SPACING 10
#define BTN_MIN_SIZE 48

/* ---- palette ----------------------------------------------------------- */
#define COLOR_PRIMARY lv_color_hex(0x0A2540)    // deep blue
#define COLOR_ACCENT lv_color_hex(0xFF6B00)     // roast orange
#define COLOR_SUCCESS lv_color_hex(0x00D084)    // normal / ok
#define COLOR_DANGER lv_color_hex(0xFF3B30)     // fault / emergency
#define COLOR_WARNING lv_color_hex(0xFFCC00)    // warning
#define COLOR_IDLE lv_color_hex(0x007AFF)       // idle / cooling
#define COLOR_HEATING lv_color_hex(0xFF6B00)

#define COLOR_BG lv_color_hex(0x0F172A)
#define COLOR_BG_CARD lv_color_hex(0x1E293B)
#define COLOR_BG_CARD_DARK lv_color_hex(0x141E30)
#define COLOR_BG_TOP_BAR lv_color_hex(0x0A1628)
#define COLOR_BG_BUTTON lv_color_hex(0x334155)

#define COLOR_TEXT_PRIMARY lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_SECONDARY lv_color_hex(0x94A3B8)
#define COLOR_TEXT_DIM lv_color_hex(0x475569)

#define COLOR_BORDER lv_color_hex(0x334155)

/* Gauge bands, in degC. The green band is the roast working range; red starts
   below the controller's 240 degC backstop so the operator sees it coming. */
#define COLOR_GAUGE_COOL lv_color_hex(0x007AFF)     // 0-150
#define COLOR_GAUGE_ROAST lv_color_hex(0x00D084)    // 150-230
#define COLOR_GAUGE_HOT lv_color_hex(0xFF6B00)      // 230-240
#define COLOR_GAUGE_DANGER lv_color_hex(0xFF3B30)   // 240-250
#define COLOR_GAUGE_BG lv_color_hex(0x1E293B)

#define COLOR_CHART_TEMP lv_color_hex(0xFF6B00)
#define COLOR_CHART_TARGET lv_color_hex(0x94A3B8)
#define COLOR_CHART_ROR lv_color_hex(0x00D084)
#define COLOR_CHART_COLOR lv_color_hex(0x007AFF)
#define COLOR_CHART_GAS lv_color_hex(0xA855F7)

/* ---- fonts ------------------------------------------------------------- */
#define FONT_SMALL &lv_font_montserrat_16
#define FONT_NORMAL &lv_font_montserrat_16
#define FONT_MEDIUM &lv_font_montserrat_20
#define FONT_LARGE &lv_font_montserrat_24
#define FONT_XL &lv_font_montserrat_30
#define FONT_XXL &lv_font_montserrat_36
#define FONT_HUGE &lv_font_montserrat_48

/* ---- gauge ------------------------------------------------------------- */
#define GAUGE_TEMP_MIN 0
#define GAUGE_TEMP_MAX 250
#define GAUGE_ROAST_START 150
#define GAUGE_HOT_START 230
#define GAUGE_DANGER_START 240
#define GAUGE_ANGLE_RANGE 270
#define GAUGE_ROTATION 135

/* ---- charts ------------------------------------------------------------ */
#define CHART_POINT_COUNT 120
#define CHART_SAMPLE_MS 2000   // one point per 2 s = 4 minutes on screen

/* ---- refresh cadence --------------------------------------------------- */
/* The controller sends a state frame every LINK_STATE_PERIOD_MS (500 ms);
   updating faster than that would only redraw identical values. */
#define UI_TICK_MS 250
#define UI_SAMPLE_TICKS (CHART_SAMPLE_MS / UI_TICK_MS)

/* ---- animation --------------------------------------------------------- */
#define ANIM_SCREEN_TRANS 200
#define ANIM_VALUE_UPDATE 150

/* ---- identity ---------------------------------------------------------- */
#define HMI_TITLE "ColAromatic"
#define HMI_SUBTITLE "Coffee roaster control panel"
#define HMI_VERSION_STRING "v1.0.0"

#endif  // UI_THEME_H
