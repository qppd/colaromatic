/*
 * analytics_screen.cpp — the roast curves.
 *
 * Four tabs: temperature against the target, rate of rise, the colour
 * sensor's clear channel and the B-688 gas resistance. One point per
 * CHART_SAMPLE_MS, and only while telemetry is actually arriving: a chart that
 * keeps drawing during a link outage would look like a measurement.
 *
 * History lives in the widgets, so switching screens and coming back starts a
 * fresh trace (the controller keeps the authoritative record in its own event
 * log/telemetry — this is an operator aid, not the record).
 */

#include "analytics_screen.h"

#include <Arduino.h>
#include <stdio.h>

#include "hmi_data.h"
#include "screen_manager.h"
#include "ui_styles.h"
#include "ui_theme.h"

static lv_obj_t *tempChart = NULL;
static lv_chart_series_t *tempSeries = NULL;
static lv_chart_series_t *targetSeries = NULL;
static lv_obj_t *rorChart = NULL;
static lv_chart_series_t *rorSeries = NULL;
static lv_obj_t *colorChart = NULL;
static lv_chart_series_t *colorSeries = NULL;
static lv_obj_t *gasChart = NULL;
static lv_chart_series_t *gasSeries = NULL;

static lv_obj_t *tempCurLabel = NULL;
static lv_obj_t *targetCurLabel = NULL;
static lv_obj_t *rorCurLabel = NULL;
static lv_obj_t *colorCurLabel = NULL;
static lv_obj_t *gasCurLabel = NULL;

static uint32_t lastSampleMs = 0;
static uint8_t lastStateSerial = 0;

static lv_obj_t *createStyledChart(lv_obj_t *parent, lv_coord_t minY,
                                   lv_coord_t maxY) {
  lv_obj_t *chart = lv_chart_create(parent);
  lv_obj_set_size(chart, LV_PCT(100), 250);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, CHART_POINT_COUNT);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, minY, maxY);
  lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_div_line_count(chart, 5, 8);

  lv_obj_set_style_bg_color(chart, COLOR_BG_CARD, 0);
  lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(chart, COLOR_BORDER, 0);
  lv_obj_set_style_border_width(chart, 1, 0);
  lv_obj_set_style_radius(chart, 8, 0);
  lv_obj_set_style_pad_all(chart, 10, 0);
  lv_obj_set_style_line_opa(chart, LV_OPA_50, 0);
  lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
  lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);  // no data-point dots
  return chart;
}

static void fillSeries(lv_obj_t *chart, lv_chart_series_t *series, int value) {
  for (int i = 0; i < CHART_POINT_COUNT; i++) {
    lv_chart_set_next_value(chart, series, value);
  }
}

static lv_obj_t *createHeader(lv_obj_t *parent, const char *title,
                              lv_obj_t **valueOut, lv_color_t color) {
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_set_size(header, LV_PCT(100), 36);
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *titleLabel = lv_label_create(header);
  lv_label_set_text(titleLabel, title);
  lv_obj_set_style_text_font(titleLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(titleLabel, COLOR_TEXT_SECONDARY, 0);
  lv_obj_align(titleLabel, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *value = lv_label_create(header);
  lv_label_set_text(value, "--");
  lv_obj_set_style_text_font(value, FONT_LARGE, 0);
  lv_obj_set_style_text_color(value, color, 0);
  lv_obj_align(value, LV_ALIGN_RIGHT_MID, 0, 0);
  *valueOut = value;

  return header;
}

static void setupTab(lv_obj_t *tab) {
  lv_obj_set_style_bg_color(tab, COLOR_BG, 0);
  lv_obj_set_style_pad_all(tab, 8, 0);
  lv_obj_set_style_pad_row(tab, 6, 0);
  lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
}

lv_obj_t *createAnalyticsScreen() {
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_add_style(scr, &style_screen_bg, 0);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  createTopBar(scr, "ROAST CURVES", true);

  lv_obj_t *tabview = lv_tabview_create(scr, LV_DIR_TOP, 40);
  lv_obj_set_size(tabview, SCREEN_WIDTH, SCREEN_HEIGHT - TOP_BAR_HEIGHT);
  lv_obj_align(tabview, LV_ALIGN_TOP_LEFT, 0, TOP_BAR_HEIGHT);
  lv_obj_set_style_bg_color(tabview, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(tabview, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(tabview, 0, 0);

  lv_obj_t *tabBtns = lv_tabview_get_tab_btns(tabview);
  lv_obj_set_style_bg_color(tabBtns, COLOR_BG_TOP_BAR, 0);
  lv_obj_set_style_bg_opa(tabBtns, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(tabBtns, COLOR_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_color(tabBtns, COLOR_ACCENT,
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(tabBtns, COLOR_ACCENT,
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_font(tabBtns, FONT_NORMAL, 0);

  /* ---- temperature vs target ---- */
  lv_obj_t *tempTab = lv_tabview_add_tab(tabview, LV_SYMBOL_CHARGE " Temp");
  setupTab(tempTab);
  createHeader(tempTab, "Probe (\xC2\xB0" "C) / target", &tempCurLabel,
               COLOR_CHART_TEMP);
  targetCurLabel = lv_label_create(tempTab);
  lv_label_set_text(targetCurLabel, "target --");
  lv_obj_set_style_text_font(targetCurLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(targetCurLabel, COLOR_CHART_TARGET, 0);
  tempChart = createStyledChart(tempTab, 0, 260);
  tempSeries = lv_chart_add_series(tempChart, COLOR_CHART_TEMP,
                                   LV_CHART_AXIS_PRIMARY_Y);
  targetSeries = lv_chart_add_series(tempChart, COLOR_CHART_TARGET,
                                     LV_CHART_AXIS_PRIMARY_Y);
  fillSeries(tempChart, tempSeries, 0);
  fillSeries(tempChart, targetSeries, 0);

  /* ---- rate of rise ---- */
  lv_obj_t *rorTab = lv_tabview_add_tab(tabview, LV_SYMBOL_UP " RoR");
  setupTab(rorTab);
  createHeader(rorTab, "Rate of rise (\xC2\xB0" "C/min)", &rorCurLabel,
               COLOR_CHART_ROR);
  lv_obj_t *rorLimit = lv_label_create(rorTab);
  lv_label_set_text(rorLimit, "runaway limit 45 \xC2\xB0" "C/min");
  lv_obj_set_style_text_font(rorLimit, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rorLimit, COLOR_TEXT_SECONDARY, 0);
  rorChart = createStyledChart(rorTab, -30, 60);
  rorSeries = lv_chart_add_series(rorChart, COLOR_CHART_ROR,
                                  LV_CHART_AXIS_PRIMARY_Y);
  fillSeries(rorChart, rorSeries, 0);

  /* ---- colour / light ---- */
  lv_obj_t *colorTab = lv_tabview_add_tab(tabview, LV_SYMBOL_TINT " Colour");
  setupTab(colorTab);
  createHeader(colorTab, "TCS3200 clear channel (Hz)", &colorCurLabel,
               COLOR_CHART_COLOR);
  lv_obj_t *colorNote = lv_label_create(colorTab);
  lv_label_set_text(colorNote, "uncalibrated light level, not a colour value");
  lv_obj_set_style_text_font(colorNote, FONT_SMALL, 0);
  lv_obj_set_style_text_color(colorNote, COLOR_TEXT_SECONDARY, 0);
  colorChart = createStyledChart(colorTab, 0, 20000);
  colorSeries = lv_chart_add_series(colorChart, COLOR_CHART_COLOR,
                                    LV_CHART_AXIS_PRIMARY_Y);
  fillSeries(colorChart, colorSeries, 0);

  /* ---- gas resistance ---- */
  lv_obj_t *gasTab = lv_tabview_add_tab(tabview, LV_SYMBOL_BARS " Gas");
  setupTab(gasTab);
  createHeader(gasTab, "BME688 gas resistance (k\xCE\xA9)", &gasCurLabel,
               COLOR_CHART_GAS);
  lv_obj_t *gasNote = lv_label_create(gasTab);
  lv_label_set_text(gasNote, "roast progress proxy, not a measurement");
  lv_obj_set_style_text_font(gasNote, FONT_SMALL, 0);
  lv_obj_set_style_text_color(gasNote, COLOR_TEXT_SECONDARY, 0);
  gasChart = createStyledChart(gasTab, 0, 1000);
  gasSeries = lv_chart_add_series(gasChart, COLOR_CHART_GAS,
                                  LV_CHART_AXIS_PRIMARY_Y);
  fillSeries(gasChart, gasSeries, 0);

  lastSampleMs = 0;
  lastStateSerial = 0;
  return scr;
}

void updateAnalyticsScreen() {
  if (!lv_obj_is_valid(tempChart)) return;

  LinkState st;
  bool connected = false;
  uint8_t serial = 0;
  hmiSnapshot(st, connected, serial);

  /* Current-value readouts follow the telemetry even between samples. */
  if (lv_obj_is_valid(tempCurLabel)) {
    if (connected && st.tempC10 != INT16_MIN) {
      lv_label_set_text_fmt(tempCurLabel, "%.1f",
                            (double)(st.tempC10 / 10.0f));
    } else {
      lv_label_set_text(tempCurLabel, "--");
    }
  }
  if (lv_obj_is_valid(targetCurLabel)) {
    if (connected && st.setpointC10 != INT16_MIN) {
      lv_label_set_text_fmt(targetCurLabel, "target %.1f",
                            (double)(st.setpointC10 / 10.0f));
    } else {
      lv_label_set_text(targetCurLabel, "target --");
    }
  }
  if (lv_obj_is_valid(rorCurLabel)) {
    if (connected && st.rorCpm10 != INT16_MIN) {
      lv_label_set_text_fmt(rorCurLabel, "%+.1f",
                            (double)(st.rorCpm10 / 10.0f));
    } else {
      lv_label_set_text(rorCurLabel, "--");
    }
  }
  if (lv_obj_is_valid(colorCurLabel)) {
    lv_label_set_text_fmt(colorCurLabel, "%lu",
                          (unsigned long)st.clearHz);
  }
  if (lv_obj_is_valid(gasCurLabel)) {
    if (connected && st.gasOhmMilli > 0) {
      lv_label_set_text_fmt(gasCurLabel, "%.1f",
                            (double)(st.gasOhmMilli / 1.0e6f));
    } else {
      lv_label_set_text(gasCurLabel, "n/a");
    }
  }

  /* One point per CHART_SAMPLE_MS, and only from a fresh frame. */
  const uint32_t now = millis();
  if (!connected) return;
  if (lastSampleMs != 0 && (now - lastSampleMs) < CHART_SAMPLE_MS) return;
  if (serial == lastStateSerial) return;  // same frame, no new data
  lastSampleMs = now;
  lastStateSerial = serial;

  const lv_coord_t temp = (st.tempC10 == INT16_MIN)
                              ? 0
                              : (lv_coord_t)(st.tempC10 / 10);
  const lv_coord_t target = (st.setpointC10 == INT16_MIN)
                                ? 0
                                : (lv_coord_t)(st.setpointC10 / 10);
  const lv_coord_t ror = (st.rorCpm10 == INT16_MIN)
                             ? 0
                             : (lv_coord_t)(st.rorCpm10 / 10);
  lv_chart_set_next_value(tempChart, tempSeries, temp);
  lv_chart_set_next_value(tempChart, targetSeries, target);
  lv_chart_set_next_value(rorChart, rorSeries, ror);
  lv_chart_set_next_value(colorChart, colorSeries, (lv_coord_t)st.clearHz);

  /* Gas spans decades, so the axis follows the reading (floor 100 kOhm). */
  lv_coord_t gasK = (st.gasOhmMilli > 0)
                        ? (lv_coord_t)(st.gasOhmMilli / 1.0e6f)
                        : 0;
  if (gasK < 0) gasK = 0;
  lv_coord_t maxK = gasK * 2;
  if (maxK < 100) maxK = 100;
  lv_chart_set_range(gasChart, LV_CHART_AXIS_PRIMARY_Y, 0, maxK);
  lv_chart_set_next_value(gasChart, gasSeries, gasK);

  lv_chart_refresh(tempChart);
  lv_chart_refresh(rorChart);
  lv_chart_refresh(colorChart);
  lv_chart_refresh(gasChart);
}
