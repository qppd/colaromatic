/*
 * dashboard_screen.cpp — the screen the operator leaves up during a roast.
 *
 * Left: the probe temperature on a 0-250 degC gauge with the roast band marked,
 * the target and the session timer. Right: rate of rise, heater duty and stage,
 * plus the colour/temperature quality line. Bottom: the four actions that must
 * be reachable in one tap (START, HOLD/RESUME, STOP & COOL, E-STOP).
 *
 * Every value is a copy of the controller's own telemetry; when the link is
 * down the numbers are replaced by "--" rather than showing the last frame as
 * if it were live (the controller has already dropped heater authority).
 */

#include "dashboard_screen.h"

#include <Arduino.h>
#include <stdio.h>

#include "alert_popup.h"
#include "esp_now_link.h"
#include "hmi_data.h"
#include "screen_manager.h"
#include "ui_styles.h"
#include "ui_theme.h"

static lv_obj_t *tempMeter = NULL;
static lv_meter_indicator_t *tempNeedle = NULL;
static lv_obj_t *tempValueLabel = NULL;
static lv_obj_t *targetLabel = NULL;
static lv_obj_t *elapsedLabel = NULL;
static lv_obj_t *stageValueLabel = NULL;
static lv_obj_t *stageMethodLabel = NULL;
static lv_obj_t *rorValueLabel = NULL;
static lv_obj_t *dutyValueLabel = NULL;
static lv_obj_t *dutyBar = NULL;
static lv_obj_t *qualityLabel = NULL;
static lv_obj_t *profileLabel = NULL;
static lv_obj_t *stateLabel = NULL;
static lv_obj_t *connIcon = NULL;
static lv_obj_t *endpointLabel = NULL;
static lv_obj_t *startBtn = NULL;
static lv_obj_t *holdBtn = NULL;
static lv_obj_t *resumeBtn = NULL;
static lv_obj_t *coolBtn = NULL;

static void setHidden(lv_obj_t *obj, bool hidden) {
  if (!lv_obj_is_valid(obj)) return;
  if (hidden) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

static void fmtClock(uint32_t ms, char *out, size_t n) {
  const uint32_t totalSec = ms / 1000UL;
  const uint32_t h = totalSec / 3600UL;
  const uint32_t m = (totalSec % 3600UL) / 60UL;
  const uint32_t s = totalSec % 60UL;
  if (h > 0) {
    snprintf(out, n, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m,
             (unsigned long)s);
  } else {
    snprintf(out, n, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
  }
}

/* ---- navigation --------------------------------------------------------- */
static void navControlCb(lv_event_t *e) { (void)e; loadScreen(SCREEN_CONTROL); }
static void navAnalyticsCb(lv_event_t *e) { (void)e; loadScreen(SCREEN_ANALYTICS); }
static void navDiagnosticsCb(lv_event_t *e) { (void)e; loadScreen(SCREEN_DIAGNOSTICS); }

/* ---- actions ------------------------------------------------------------ */
static void startCb(lv_event_t *e) {
  (void)e;
  uint8_t profile, mode, duty;
  int16_t setpointC10;
  hmiOperatorSnapshot(profile, mode, setpointC10, duty);
  if (profile == LINK_PROFILE_NONE) {
    showAlert("NO PROFILE", "Select a bean profile on the CONTROL screen first.",
              ALERT_WARNING);
    return;
  }
  if (!hmiCmdStart()) {
    showAlert("LINK DOWN", "Could not send START: no controller link.",
              ALERT_ERROR);
  }
}

static void holdCb(lv_event_t *e) { (void)e; hmiCmdHold(); }
static void resumeCb(lv_event_t *e) { (void)e; hmiCmdResume(); }
static void coolCb(lv_event_t *e) { (void)e; hmiCmdCool(); }
static void estopCb(lv_event_t *e) { (void)e; hmiCmdEstop(); }

/* ---- widgets ------------------------------------------------------------ */

static lv_obj_t *createTempGauge(lv_obj_t *parent) {
  lv_obj_t *meter = lv_meter_create(parent);
  lv_obj_set_size(meter, 250, 250);
  lv_obj_set_style_bg_color(meter, COLOR_GAUGE_BG, 0);
  lv_obj_set_style_border_width(meter, 0, 0);
  lv_obj_set_style_pad_all(meter, 10, 0);

  lv_meter_scale_t *scale = lv_meter_add_scale(meter);
  lv_meter_set_scale_ticks(meter, scale, 26, 2, 6, COLOR_TEXT_SECONDARY);
  lv_meter_set_scale_major_ticks(meter, scale, 5, 3, 12, COLOR_TEXT_PRIMARY, 12);
  lv_meter_set_scale_range(meter, scale, GAUGE_TEMP_MIN, GAUGE_TEMP_MAX,
                           GAUGE_ANGLE_RANGE, GAUGE_ROTATION);

  struct Band {
    int from;
    int to;
    lv_color_t color;
  };
  const Band bands[] = {
      {GAUGE_TEMP_MIN, GAUGE_ROAST_START, COLOR_GAUGE_COOL},
      {GAUGE_ROAST_START, GAUGE_HOT_START, COLOR_GAUGE_ROAST},
      {GAUGE_HOT_START, GAUGE_DANGER_START, COLOR_GAUGE_HOT},
      {GAUGE_DANGER_START, GAUGE_TEMP_MAX, COLOR_GAUGE_DANGER},
  };
  for (size_t i = 0; i < sizeof(bands) / sizeof(bands[0]); i++) {
    lv_meter_indicator_t *arc =
        lv_meter_add_arc(meter, scale, 6, bands[i].color, -2);
    lv_meter_set_indicator_start_value(meter, arc, bands[i].from);
    lv_meter_set_indicator_end_value(meter, arc, bands[i].to);
  }

  tempNeedle = lv_meter_add_needle_line(meter, scale, 3, COLOR_ACCENT, -16);
  lv_meter_set_indicator_value(meter, tempNeedle, 0);
  return meter;
}

static lv_obj_t *createValueCard(lv_obj_t *parent, const char *title,
                                 lv_obj_t **valueOut, lv_color_t valueColor,
                                 lv_coord_t w) {
  lv_obj_t *card = createCard(parent, w, 120);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(card, 4, 0);

  lv_obj_t *value = lv_label_create(card);
  lv_label_set_text(value, "--");
  lv_obj_set_style_text_font(value, FONT_XL, 0);
  lv_obj_set_style_text_color(value, valueColor, 0);
  *valueOut = value;

  lv_obj_t *titleLabel = lv_label_create(card);
  lv_label_set_text(titleLabel, title);
  lv_obj_add_style(titleLabel, &style_text_label, 0);

  return card;
}

lv_obj_t *createDashboardScreen() {
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_add_style(scr, &style_screen_bg, 0);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  /* ---------------- top bar ---------------- */
  lv_obj_t *topBar = createTopBar(scr, HMI_TITLE, false);

  connIcon = lv_label_create(topBar);
  lv_label_set_text(connIcon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(connIcon, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(connIcon, COLOR_TEXT_SECONDARY, 0);

  stateLabel = lv_label_create(topBar);
  lv_label_set_text(stateLabel, "OFFLINE");
  lv_obj_set_style_text_font(stateLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(stateLabel, COLOR_DANGER, 0);

  /* ---------------- content ---------------- */
  lv_obj_t *content = lv_obj_create(scr);
  lv_obj_set_size(content, SCREEN_WIDTH, CONTENT_HEIGHT);
  lv_obj_align(content, LV_ALIGN_TOP_LEFT, 0, TOP_BAR_HEIGHT);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, SIDE_PADDING, 0);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

  /* left: gauge + readouts */
  lv_obj_t *gaugeBox = lv_obj_create(content);
  lv_obj_set_size(gaugeBox, 290, CONTENT_HEIGHT - 2 * SIDE_PADDING);
  lv_obj_align(gaugeBox, LV_ALIGN_LEFT_TOP, 0, 0);
  lv_obj_set_style_bg_opa(gaugeBox, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(gaugeBox, 0, 0);
  lv_obj_set_style_pad_all(gaugeBox, 0, 0);
  lv_obj_set_scrollbar_mode(gaugeBox, LV_SCROLLBAR_MODE_OFF);

  tempMeter = createTempGauge(gaugeBox);
  lv_obj_align(tempMeter, LV_ALIGN_TOP_MID, 0, 0);

  tempValueLabel = lv_label_create(gaugeBox);
  lv_label_set_text(tempValueLabel, "--");
  lv_obj_set_style_text_font(tempValueLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(tempValueLabel, COLOR_TEXT_PRIMARY, 0);
  lv_obj_align(tempValueLabel, LV_ALIGN_BOTTOM_MID, 0, -58);

  targetLabel = lv_label_create(gaugeBox);
  lv_label_set_text(targetLabel, "target --");
  lv_obj_set_style_text_font(targetLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(targetLabel, COLOR_ACCENT, 0);
  lv_obj_align(targetLabel, LV_ALIGN_BOTTOM_MID, 0, -32);

  elapsedLabel = lv_label_create(gaugeBox);
  lv_label_set_text(elapsedLabel, "00:00");
  lv_obj_set_style_text_font(elapsedLabel, FONT_MEDIUM, 0);
  lv_obj_set_style_text_color(elapsedLabel, COLOR_TEXT_PRIMARY, 0);
  lv_obj_align(elapsedLabel, LV_ALIGN_BOTTOM_MID, 0, -4);

  /* right: cards, quality line, navigation */
  lv_obj_t *rightCol = lv_obj_create(content);
  lv_obj_set_size(rightCol, 440, CONTENT_HEIGHT - 2 * SIDE_PADDING);
  lv_obj_align(rightCol, LV_ALIGN_RIGHT_TOP, 0, 0);
  lv_obj_set_style_bg_opa(rightCol, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(rightCol, 0, 0);
  lv_obj_set_style_pad_all(rightCol, 0, 0);
  lv_obj_set_scrollbar_mode(rightCol, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *cardsRow = lv_obj_create(rightCol);
  lv_obj_set_size(cardsRow, 440, 120);
  lv_obj_align(cardsRow, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(cardsRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cardsRow, 0, 0);
  lv_obj_set_style_pad_all(cardsRow, 0, 0);
  lv_obj_set_scrollbar_mode(cardsRow, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(cardsRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cardsRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  createValueCard(cardsRow, "RoR  \xC2\xB0" "C/min", &rorValueLabel,
                  COLOR_CHART_ROR, 140);
  createValueCard(cardsRow, "Duty  %", &dutyValueLabel, COLOR_ACCENT, 140);
  lv_obj_t *stageCard = createValueCard(cardsRow, "stage", &stageValueLabel,
                                        COLOR_TEXT_PRIMARY, 140);
  stageMethodLabel = lv_label_create(stageCard);
  lv_label_set_text(stageMethodLabel, "");
  lv_obj_set_style_text_font(stageMethodLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(stageMethodLabel, COLOR_TEXT_SECONDARY, 0);

  /* Heater duty bar: the applied duty against the controller's ceiling. */
  dutyBar = lv_bar_create(rightCol);
  lv_obj_set_size(dutyBar, 440, 12);
  lv_obj_align(dutyBar, LV_ALIGN_TOP_LEFT, 0, 126);
  lv_bar_set_range(dutyBar, 0, 100);
  lv_bar_set_value(dutyBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(dutyBar, COLOR_BG_CARD, 0);
  lv_obj_set_style_bg_color(dutyBar, COLOR_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(dutyBar, 6, 0);
  lv_obj_set_style_radius(dutyBar, 6, LV_PART_INDICATOR);

  lv_obj_t *infoCard = createCard(rightCol, 440, 96);
  lv_obj_align(infoCard, LV_ALIGN_TOP_LEFT, 0, 146);
  lv_obj_set_flex_flow(infoCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(infoCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(infoCard, 2, 0);

  profileLabel = lv_label_create(infoCard);
  lv_label_set_text(profileLabel, "profile: --");
  lv_obj_set_style_text_font(profileLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(profileLabel, COLOR_TEXT_PRIMARY, 0);

  qualityLabel = lv_label_create(infoCard);
  lv_label_set_text(qualityLabel, "temp --   colour --");
  lv_obj_set_style_text_font(qualityLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(qualityLabel, COLOR_TEXT_SECONDARY, 0);

  endpointLabel = lv_label_create(infoCard);
  lv_label_set_text(endpointLabel, "");
  lv_obj_set_style_text_font(endpointLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(endpointLabel, COLOR_WARNING, 0);
  setHidden(endpointLabel, true);

  lv_obj_t *navRow = lv_obj_create(rightCol);
  lv_obj_set_size(navRow, 440, 52);
  lv_obj_align(navRow, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(navRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(navRow, 0, 0);
  lv_obj_set_style_pad_all(navRow, 0, 0);
  lv_obj_set_scrollbar_mode(navRow, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *ctrlBtn =
      createButton(navRow, LV_SYMBOL_SETTINGS " CONTROL", 140, BTN_MIN_SIZE,
                   &style_btn_nav);
  lv_obj_add_event_cb(ctrlBtn, navControlCb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *chartBtn =
      createButton(navRow, LV_SYMBOL_LIST " CURVES", 140, BTN_MIN_SIZE,
                   &style_btn_nav);
  lv_obj_add_event_cb(chartBtn, navAnalyticsCb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *diagBtn =
      createButton(navRow, LV_SYMBOL_EYE_OPEN " DIAG", 130, BTN_MIN_SIZE,
                   &style_btn_nav);
  lv_obj_add_event_cb(diagBtn, navDiagnosticsCb, LV_EVENT_CLICKED, NULL);

  /* ---------------- bottom bar: the four one-tap actions ---------------- */
  lv_obj_t *bottomBar = lv_obj_create(scr);
  lv_obj_set_size(bottomBar, SCREEN_WIDTH, BOTTOM_BAR_HEIGHT);
  lv_obj_align(bottomBar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(bottomBar, COLOR_BG_TOP_BAR, 0);
  lv_obj_set_style_bg_opa(bottomBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(bottomBar, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(bottomBar, 1, 0);
  lv_obj_set_style_border_color(bottomBar, COLOR_BORDER, 0);
  lv_obj_set_style_radius(bottomBar, 0, 0);
  lv_obj_set_style_pad_hor(bottomBar, SIDE_PADDING, 0);
  lv_obj_set_scrollbar_mode(bottomBar, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(bottomBar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bottomBar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  startBtn = createButton(bottomBar, LV_SYMBOL_PLAY " START", 170, 44,
                          &style_btn_success);
  lv_obj_add_event_cb(startBtn, startCb, LV_EVENT_CLICKED, NULL);

  holdBtn = createButton(bottomBar, LV_SYMBOL_PAUSE " HOLD", 150, 44,
                         &style_btn_nav);
  lv_obj_add_event_cb(holdBtn, holdCb, LV_EVENT_CLICKED, NULL);

  resumeBtn = createButton(bottomBar, LV_SYMBOL_PLAY " RESUME", 150, 44,
                           &style_btn_success);
  lv_obj_add_event_cb(resumeBtn, resumeCb, LV_EVENT_CLICKED, NULL);

  coolBtn = createButton(bottomBar, LV_SYMBOL_STOP " STOP & COOL", 170, 44,
                         &style_btn_primary);
  lv_obj_add_event_cb(coolBtn, coolCb, LV_EVENT_CLICKED, NULL);

  /* Emergency stop is never hidden and never asks for confirmation: the
     controller's own E-stop input is the primary protection, this is the
     software path the operator can reach from the panel. */
  lv_obj_t *estopBtn = createButton(bottomBar, LV_SYMBOL_WARNING " E-STOP", 150,
                                    44, &style_btn_danger);
  lv_obj_add_event_cb(estopBtn, estopCb, LV_EVENT_CLICKED, NULL);

  return scr;
}

void updateDashboardScreen() {
  if (!lv_obj_is_valid(tempMeter)) return;

  LinkState st;
  bool connected = false;
  uint8_t serial = 0;
  hmiSnapshot(st, connected, serial);
  (void)serial;  // this screen has no chart sampling

  uint8_t profile, mode, dutySel;
  int16_t setpointC10;
  hmiOperatorSnapshot(profile, mode, setpointC10, dutySel);
  (void)dutySel;  // manual duty is edited on the control screen

  /* ---- top bar ---- */
  if (lv_obj_is_valid(connIcon)) {
    lv_obj_set_style_text_color(connIcon,
                                connected ? COLOR_SUCCESS : COLOR_DANGER, 0);
  }
  if (lv_obj_is_valid(stateLabel)) {
    lv_label_set_text(stateLabel,
                      connected ? hmiRoastStateText(st.roastState) : "OFFLINE");
    lv_obj_set_style_text_color(
        stateLabel, connected ? getStateColor(st.roastState) : COLOR_DANGER, 0);
  }

  /* ---- temperature ---- */
  const bool haveTemp = connected && st.tempC10 != INT16_MIN;
  if (lv_obj_is_valid(tempNeedle)) {
    int gauge = GAUGE_TEMP_MIN;
    if (haveTemp) {
      gauge = st.tempC10 / 10;
      if (gauge < GAUGE_TEMP_MIN) gauge = GAUGE_TEMP_MIN;
      if (gauge > GAUGE_TEMP_MAX) gauge = GAUGE_TEMP_MAX;
    }
    lv_meter_set_indicator_value(tempMeter, tempNeedle, gauge);
  }
  if (lv_obj_is_valid(tempValueLabel)) {
    if (haveTemp) {
      lv_label_set_text_fmt(tempValueLabel, "%.1f \xC2\xB0" "C",
                            (double)(st.tempC10 / 10.0f));
      lv_obj_set_style_text_color(tempValueLabel, COLOR_TEXT_PRIMARY, 0);
    } else {
      lv_label_set_text(tempValueLabel, "--");
      lv_obj_set_style_text_color(tempValueLabel, COLOR_TEXT_SECONDARY, 0);
    }
  }
  if (lv_obj_is_valid(targetLabel)) {
    if (connected && st.setpointC10 != INT16_MIN) {
      lv_label_set_text_fmt(targetLabel, "target %.1f \xC2\xB0" "C",
                            (double)(st.setpointC10 / 10.0f));
    } else {
      lv_label_set_text(targetLabel, "target --");
    }
  }
  if (lv_obj_is_valid(elapsedLabel)) {
    if (connected && st.sessionMs > 0) {
      char buf[16];
      fmtClock(st.sessionMs, buf, sizeof(buf));
      lv_label_set_text(elapsedLabel, buf);
    } else {
      lv_label_set_text(elapsedLabel, "00:00");
    }
  }

  /* ---- cards ---- */
  if (lv_obj_is_valid(rorValueLabel)) {
    if (connected && st.rorCpm10 != INT16_MIN) {
      lv_label_set_text_fmt(rorValueLabel, "%+.1f",
                            (double)(st.rorCpm10 / 10.0f));
    } else {
      lv_label_set_text(rorValueLabel, "--");
    }
  }
  if (lv_obj_is_valid(dutyValueLabel)) {
    if (connected) {
      lv_label_set_text_fmt(dutyValueLabel, "%u/%u",
                            (unsigned)st.dutyApp, (unsigned)st.dutyCmd);
    } else {
      lv_label_set_text(dutyValueLabel, "--");
    }
  }
  if (lv_obj_is_valid(dutyBar)) {
    lv_bar_set_value(dutyBar, connected ? (int)st.dutyApp : 0, LV_ANIM_OFF);
  }
  if (lv_obj_is_valid(stageValueLabel)) {
    lv_label_set_text(stageValueLabel,
                      connected ? hmiStageText(st.stage) : "--");
  }
  if (lv_obj_is_valid(stageMethodLabel)) {
    if (connected) {
      lv_label_set_text_fmt(stageMethodLabel, "%s \xC2\xB7 %s",
                            hmiStageMethodText(st.stageMethod),
                            st.stageConfirmed ? "confirmed" : "estimated");
    } else {
      lv_label_set_text(stageMethodLabel, "");
    }
  }

  /* ---- info card ---- */
  if (lv_obj_is_valid(profileLabel)) {
    const uint8_t idx = connected ? st.profileIdx : profile;
    if (idx < GEN_PROFILE_COUNT) {
      lv_label_set_text_fmt(profileLabel, "%s  \xC2\xB7  %s",
                            GEN_PROFILES[idx].label, hmiModeText(mode));
      lv_obj_set_style_text_color(profileLabel, COLOR_TEXT_PRIMARY, 0);
    } else {
      lv_label_set_text(profileLabel, "no profile selected");
      lv_obj_set_style_text_color(profileLabel, COLOR_WARNING, 0);
    }
  }
  if (lv_obj_is_valid(qualityLabel)) {
    if (connected) {
      lv_label_set_text_fmt(qualityLabel, "temp %s   colour %s   gas %s",
                            hmiTempQualityText(st.tempQuality),
                            hmiColorStatusText(st.colorStatus),
                            st.gasOhmMilli == INT32_MIN ? "n/a" : "ok");
    } else {
      lv_label_set_text(qualityLabel, "no telemetry");
    }
  }
  if (lv_obj_is_valid(endpointLabel)) {
    const bool pending = connected && st.endpointPending;
    setHidden(endpointLabel, !pending);
    if (pending) {
      lv_label_set_text(endpointLabel,
                        "ENDPOINT PENDING \xC2\xB7 confirm on CONTROL");
    }
  }

  /* ---- action visibility ---- */
  const uint8_t rs = connected ? st.roastState : ROAST_IDLE;
  const bool live = (rs == ROAST_RUNNING || rs == ROAST_HOLD ||
                     rs == ROAST_COOLING);
  setHidden(startBtn, live);
  setHidden(holdBtn, !(rs == ROAST_RUNNING));
  setHidden(resumeBtn, !(rs == ROAST_HOLD));
  setHidden(coolBtn, !(rs == ROAST_RUNNING || rs == ROAST_HOLD));

  /* E-STOP stays available even offline: if the link is down the operator can
     still use the machine's physical E-stop, and this button is harmless. */
}
