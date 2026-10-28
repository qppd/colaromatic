/*
 * diagnostics_screen.cpp — link, sensor and fault detail.
 *
 * This is the screen a commissioning engineer leaves open: the MACs (needed to
 * fill in HMI_PEER_MAC on the controller), the age of the last telemetry
 * frame, the state of every input the controller reports, the active and
 * latched fault masks, the controller's recent events, and the two service
 * actions (clear latches, re-pair).
 */

#include "diagnostics_screen.h"

#include <Arduino.h>
#include <stdio.h>

#include "alert_popup.h"
#include "esp_now_link.h"
#include "hmi_data.h"
#include "screen_manager.h"
#include "ui_styles.h"
#include "ui_theme.h"

#define DIAG_EVENT_ROWS 6

static lv_obj_t *linkDot = NULL;
static lv_obj_t *linkValue = NULL;
static lv_obj_t *peerMacValue = NULL;
static lv_obj_t *selfMacValue = NULL;
static lv_obj_t *pairedValue = NULL;
static lv_obj_t *ageValue = NULL;
static lv_obj_t *tempValue = NULL;
static lv_obj_t *colorValue = NULL;
static lv_obj_t *gasValue = NULL;
static lv_obj_t *dutyValue = NULL;
static lv_obj_t *activeFaultsValue = NULL;
static lv_obj_t *latchedFaultsValue = NULL;
static lv_obj_t *eventRows[DIAG_EVENT_ROWS] = {NULL};
static lv_obj_t *hmiInfoValue = NULL;

static void setValueColor(lv_obj_t *label, lv_color_t color) {
  if (lv_obj_is_valid(label)) lv_obj_set_style_text_color(label, color, 0);
}

/* ---- service actions ---------------------------------------------------- */

static void resetFaultsCb(lv_event_t *e) {
  (void)e;
  if (!hmiCmdReset()) {
    showAlert("LINK DOWN", "Could not send the reset: no controller link.",
              ALERT_ERROR);
  }
  /* A refusal (still hot, E-stop held) comes back as an ack and is shown by
     the alert system with the controller's own reason text. */
}

static void pairCb(lv_event_t *e) {
  (void)e;
  linkOpenPairing(millis());
  showAlert("PAIRING",
            "Pairing request sent.\nThe panel is now advertising for a controller.",
            ALERT_INFO);
}

/* ---- widgets ------------------------------------------------------------ */

static lv_obj_t *createInfoRow(lv_obj_t *parent, const char *label,
                               const char *initial) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), 30);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *lbl = lv_label_create(row);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(lbl, COLOR_TEXT_SECONDARY, 0);

  lv_obj_t *value = lv_label_create(row);
  lv_label_set_text(value, initial);
  lv_obj_set_style_text_font(value, FONT_SMALL, 0);
  lv_obj_set_style_text_color(value, COLOR_TEXT_PRIMARY, 0);
  return value;
}

static lv_obj_t *createSectionTitle(lv_obj_t *parent, const char *text) {
  lv_obj_t *title = lv_label_create(parent);
  lv_label_set_text(title, text);
  lv_obj_add_style(title, &style_text_label, 0);
  lv_obj_set_style_text_letter_space(title, 2, 0);
  return title;
}

lv_obj_t *createDiagnosticsScreen() {
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_add_style(scr, &style_screen_bg, 0);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  createTopBar(scr, "DIAGNOSTICS", true);

  lv_obj_t *content = lv_obj_create(scr);
  lv_obj_set_size(content, SCREEN_WIDTH, SCREEN_HEIGHT - TOP_BAR_HEIGHT);
  lv_obj_align(content, LV_ALIGN_TOP_LEFT, 0, TOP_BAR_HEIGHT);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, SIDE_PADDING, 0);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  /* ---------------- left: link + inputs ---------------- */
  lv_obj_t *leftCard = createCard(content, 370, 400);
  lv_obj_set_flex_flow(leftCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(leftCard, 2, 0);

  createSectionTitle(leftCard, "LINK");
  /* Link status row with a dot: built explicitly so the dot can be recoloured
     without rebuilding the row. */
  lv_obj_t *linkRow = lv_obj_create(leftCard);
  lv_obj_set_size(linkRow, LV_PCT(100), 30);
  lv_obj_set_style_bg_opa(linkRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(linkRow, 0, 0);
  lv_obj_set_style_pad_all(linkRow, 0, 0);
  lv_obj_set_scrollbar_mode(linkRow, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(linkRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(linkRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(linkRow, 8, 0);
  linkDot = createStatusDot(linkRow, 12);
  linkValue = lv_label_create(linkRow);
  lv_label_set_text(linkValue, "offline");
  lv_obj_set_style_text_font(linkValue, FONT_SMALL, 0);
  lv_obj_set_style_text_color(linkValue, COLOR_TEXT_PRIMARY, 0);

  pairedValue = createInfoRow(leftCard, "Paired", "no");
  selfMacValue = createInfoRow(leftCard, "Panel MAC", "-");
  peerMacValue = createInfoRow(leftCard, "Controller MAC", "-");
  ageValue = createInfoRow(leftCard, "Last frame", "never");

  createDivider(leftCard);
  createSectionTitle(leftCard, "INPUTS");
  tempValue = createInfoRow(leftCard, "Thermocouple", "-");
  colorValue = createInfoRow(leftCard, "Colour sensor", "-");
  gasValue = createInfoRow(leftCard, "BME688 gas", "-");
  dutyValue = createInfoRow(leftCard, "Duty cmd/applied", "-");
  hmiInfoValue = createInfoRow(leftCard, "Panel uptime / heap", "-");

  /* ---------------- right: faults, events, actions ---------------- */
  lv_obj_t *rightCard = createCard(content, 370, 400);
  lv_obj_set_flex_flow(rightCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(rightCard, 2, 0);

  createSectionTitle(rightCard, "ACTIVE FAULTS");
  activeFaultsValue = lv_label_create(rightCard);
  lv_label_set_text(activeFaultsValue, "none");
  lv_obj_set_style_text_font(activeFaultsValue, FONT_SMALL, 0);
  lv_obj_set_style_text_color(activeFaultsValue, COLOR_TEXT_PRIMARY, 0);
  lv_obj_set_width(activeFaultsValue, LV_PCT(100));
  lv_label_set_long_mode(activeFaultsValue, LV_LABEL_LONG_WRAP);

  createSectionTitle(rightCard, "LATCHED FAULTS (need a cool reset)");
  latchedFaultsValue = lv_label_create(rightCard);
  lv_label_set_text(latchedFaultsValue, "none");
  lv_obj_set_style_text_font(latchedFaultsValue, FONT_SMALL, 0);
  lv_obj_set_style_text_color(latchedFaultsValue, COLOR_TEXT_PRIMARY, 0);
  lv_obj_set_width(latchedFaultsValue, LV_PCT(100));
  lv_label_set_long_mode(latchedFaultsValue, LV_LABEL_LONG_WRAP);

  createDivider(rightCard);
  createSectionTitle(rightCard, "CONTROLLER EVENTS");
  for (int i = 0; i < DIAG_EVENT_ROWS; i++) {
    eventRows[i] = lv_label_create(rightCard);
    lv_label_set_text(eventRows[i], "");
    lv_obj_set_style_text_font(eventRows[i], FONT_SMALL, 0);
    lv_obj_set_style_text_color(eventRows[i], COLOR_TEXT_SECONDARY, 0);
  }

  lv_obj_t *btnRow = lv_obj_create(rightCard);
  lv_obj_set_size(btnRow, LV_PCT(100), 50);
  lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btnRow, 0, 0);
  lv_obj_set_style_pad_all(btnRow, 0, 0);
  lv_obj_set_scrollbar_mode(btnRow, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *resetBtn = createButton(btnRow, LV_SYMBOL_REFRESH " RESET FAULTS",
                                    170, 44, &style_btn_nav);
  lv_obj_add_event_cb(resetBtn, resetFaultsCb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *pairBtn = createButton(btnRow, LV_SYMBOL_WIFI " RE-PAIR", 160, 44,
                                   &style_btn_nav);
  lv_obj_add_event_cb(pairBtn, pairCb, LV_EVENT_CLICKED, NULL);

  return scr;
}

void updateDiagnosticsScreen() {
  if (!lv_obj_is_valid(linkValue)) return;

  LinkState st;
  bool connected = false;
  uint8_t serial = 0;
  hmiSnapshot(st, connected, serial);
  (void)serial;  // this screen does not sample charts

  char text[64];

  /* ---- link ---- */
  setDotOn(linkDot, connected, COLOR_SUCCESS);
  if (lv_obj_is_valid(linkValue)) {
    lv_label_set_text(linkValue, connected ? "connected" : "offline");
    setValueColor(linkValue, connected ? COLOR_SUCCESS : COLOR_DANGER);
  }
  if (lv_obj_is_valid(pairedValue)) {
    lv_label_set_text(pairedValue, linkPaired() ? "yes" : "no");
    setValueColor(pairedValue, linkPaired() ? COLOR_TEXT_PRIMARY : COLOR_WARNING);
  }
  if (lv_obj_is_valid(selfMacValue)) {
    uint8_t self[6];
    linkSelfMac(self);
    linkMacText(self, text, sizeof(text));
    lv_label_set_text(selfMacValue, text);
  }
  if (lv_obj_is_valid(peerMacValue)) {
    linkMacText(linkPeerMac(), text, sizeof(text));
    lv_label_set_text(peerMacValue, text);
  }
  if (lv_obj_is_valid(ageValue)) {
    const uint32_t last = linkLastRxMs();
    if (last == 0) {
      lv_label_set_text(ageValue, "never");
    } else {
      const uint32_t age = millis() - last;
      if (age < 1500) {
        lv_label_set_text(ageValue, "just now");
      } else {
        lv_label_set_text_fmt(ageValue, "%lu s ago",
                              (unsigned long)(age / 1000UL));
      }
      setValueColor(ageValue, connected ? COLOR_TEXT_PRIMARY : COLOR_DANGER);
    }
  }

  /* ---- inputs ---- */
  if (lv_obj_is_valid(tempValue)) {
    if (!connected) {
      lv_label_set_text(tempValue, "no link");
      setValueColor(tempValue, COLOR_DANGER);
    } else if (st.tempC10 == INT16_MIN) {
      lv_label_set_text(tempValue, hmiTempQualityText(st.tempQuality));
      setValueColor(tempValue, COLOR_DANGER);
    } else {
      lv_label_set_text_fmt(tempValue, "%.2f \xC2\xB0" "C \xC2\xB7 %s",
                            (double)(st.tempC10 / 10.0f),
                            hmiTempQualityText(st.tempQuality));
      setValueColor(tempValue,
                    st.tempQuality == TEMP_OK ? COLOR_TEXT_PRIMARY
                                              : COLOR_WARNING);
    }
  }
  if (lv_obj_is_valid(colorValue)) {
    snprintf(text, sizeof(text), "%s", hmiColorStatusText(st.colorStatus));
    lv_label_set_text_fmt(colorValue, "%s \xC2\xB7 %lu Hz", text,
                          (unsigned long)st.clearHz);
    setValueColor(colorValue, st.colorStatus == COLOR_INVALID ? COLOR_DANGER
                                                             : COLOR_WARNING);
  }
  if (lv_obj_is_valid(gasValue)) {
    if (st.gasOhmMilli == INT32_MIN) {
      lv_label_set_text(gasValue, "not available");
      setValueColor(gasValue, COLOR_TEXT_SECONDARY);
    } else {
      lv_label_set_text_fmt(gasValue, "%.2f k\xCE\xA9",
                            (double)(st.gasOhmMilli / 1.0e6f));
      setValueColor(gasValue, COLOR_TEXT_PRIMARY);
    }
  }
  if (lv_obj_is_valid(dutyValue)) {
    lv_label_set_text_fmt(dutyValue, "%u%% / %u%%  relay %s",
                          (unsigned)st.dutyCmd, (unsigned)st.dutyApp,
                          st.relayOn ? "on" : "off");
  }
  if (lv_obj_is_valid(hmiInfoValue)) {
    lv_label_set_text_fmt(hmiInfoValue, "%lu s / %lu KB",
                          (unsigned long)(millis() / 1000UL),
                          (unsigned long)(ESP.getFreeHeap() / 1024));
  }

  /* ---- faults ---- */
  if (lv_obj_is_valid(activeFaultsValue)) {
    if (st.faultsActive == 0) {
      lv_label_set_text(activeFaultsValue, "none");
      setValueColor(activeFaultsValue, COLOR_SUCCESS);
    } else {
      hmiFaultText(st.faultsActive, text, sizeof(text));
      lv_label_set_text(activeFaultsValue, text);
      setValueColor(activeFaultsValue, COLOR_DANGER);
    }
  }
  if (lv_obj_is_valid(latchedFaultsValue)) {
    if (st.faultsLatched == 0) {
      lv_label_set_text(latchedFaultsValue, "none");
      setValueColor(latchedFaultsValue, COLOR_SUCCESS);
    } else {
      hmiFaultText(st.faultsLatched, text, sizeof(text));
      lv_label_set_text(latchedFaultsValue, text);
      setValueColor(latchedFaultsValue, COLOR_WARNING);
    }
  }

  /* ---- events, newest first ---- */
  HmiEvent events[DIAG_EVENT_ROWS];
  const uint8_t count = hmiCopyEvents(events, DIAG_EVENT_ROWS);
  for (int i = 0; i < DIAG_EVENT_ROWS; i++) {
    if (!lv_obj_is_valid(eventRows[i])) continue;
    if (i >= count) {
      lv_label_set_text(eventRows[i], "");
      continue;
    }
    const HmiEvent &ev = events[i];
    lv_label_set_text_fmt(eventRows[i], "%5lus  %-9s %s",
                          (unsigned long)(ev.sessionMs / 1000UL),
                          hmiEventText(ev.type), ev.text);
  }
}
