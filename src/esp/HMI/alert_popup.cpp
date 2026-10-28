#include "alert_popup.h"

#include <Arduino.h>
#include <stdio.h>

#include "hmi_data.h"
#include "ui_styles.h"
#include "ui_theme.h"

static lv_obj_t *activeAlert = NULL;
static uint32_t lastLinkAlertMs = 0;
static uint32_t lastEventAtMs = 0;
static uint8_t lastAckSerial = 0;
static bool sawLink = false;
static uint8_t lastStateSerial = 0;
static uint8_t lastRoastState = ROAST_IDLE;

static void alertDismissCb(lv_event_t *e) {
  (void)e;
  dismissAlert();
}

void alertInit() {
  activeAlert = NULL;
  lastLinkAlertMs = 0;
  lastEventAtMs = 0;
  lastAckSerial = 0;
  sawLink = false;
  lastStateSerial = 0;
  lastRoastState = ROAST_IDLE;
}

bool alertIsOpen() { return activeAlert != NULL; }

void dismissAlert() {
  if (activeAlert != NULL) {
    lv_obj_del(activeAlert);
    activeAlert = NULL;
  }
}

void showAlert(const char *title, const char *message, AlertType type) {
  dismissAlert();

  lv_color_t color;
  const char *icon;
  switch (type) {
    case ALERT_WARNING:
      color = COLOR_WARNING;
      icon = LV_SYMBOL_WARNING;
      break;
    case ALERT_ERROR:
      color = COLOR_DANGER;
      icon = LV_SYMBOL_CLOSE;
      break;
    case ALERT_SUCCESS:
      color = COLOR_SUCCESS;
      icon = LV_SYMBOL_OK;
      break;
    default:
      color = COLOR_IDLE;
      icon = LV_SYMBOL_BELL;
      break;
  }

  lv_obj_t *overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_set_style_radius(overlay, 0, 0);
  lv_obj_set_scrollbar_mode(overlay, LV_SCROLLBAR_MODE_OFF);
  activeAlert = overlay;

  lv_obj_t *box = lv_obj_create(overlay);
  lv_obj_set_size(box, 460, 260);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, COLOR_BG_CARD, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, 16, 0);
  lv_obj_set_style_border_width(box, 2, 0);
  lv_obj_set_style_border_color(box, color, 0);
  lv_obj_set_style_pad_all(box, 18, 0);
  lv_obj_set_style_pad_row(box, 10, 0);
  lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_shadow_color(box, color, 0);
  lv_obj_set_style_shadow_width(box, 24, 0);
  lv_obj_set_style_shadow_opa(box, LV_OPA_30, 0);

  lv_obj_t *iconLbl = lv_label_create(box);
  lv_label_set_text(iconLbl, icon);
  lv_obj_set_style_text_font(iconLbl, FONT_XXL, 0);
  lv_obj_set_style_text_color(iconLbl, color, 0);

  lv_obj_t *titleLbl = lv_label_create(box);
  lv_label_set_text(titleLbl, title);
  lv_obj_set_style_text_font(titleLbl, FONT_LARGE, 0);
  lv_obj_set_style_text_color(titleLbl, COLOR_TEXT_PRIMARY, 0);

  lv_obj_t *msgLbl = lv_label_create(box);
  lv_label_set_text(msgLbl, message);
  lv_obj_set_style_text_font(msgLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(msgLbl, COLOR_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_align(msgLbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(msgLbl, 400);

  lv_obj_t *ackBtn = lv_btn_create(box);
  lv_obj_set_size(ackBtn, 180, 44);
  lv_obj_set_style_bg_color(ackBtn, color, 0);
  lv_obj_set_style_radius(ackBtn, 8, 0);
  lv_obj_add_event_cb(ackBtn, alertDismissCb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *ackLabel = lv_label_create(ackBtn);
  lv_label_set_text(ackLabel, "ACKNOWLEDGE");
  lv_obj_set_style_text_font(ackLabel, FONT_MEDIUM, 0);
  lv_obj_set_style_text_color(ackLabel, COLOR_TEXT_PRIMARY, 0);
  lv_obj_center(ackLabel);
}

void checkAlerts() {
  LinkState st;
  bool connected = false;
  uint8_t stateSerial = 0;
  hmiSnapshot(st, connected, stateSerial);

  if (connected) sawLink = true;

  /* 1. A new command refusal: the controller sends the reason verbatim, so
        show it rather than inventing a friendlier message. */
  LinkAck ack;
  uint8_t ackSerial = 0;
  if (hmiCopyAck(ack, ackSerial) && ackSerial != lastAckSerial) {
    lastAckSerial = ackSerial;
    if (ack.result != ACK_OK && !alertIsOpen()) {
      char msg[96];
      snprintf(msg, sizeof(msg), "opcode %u: %s", (unsigned)ack.opcode,
               ack.text);
      showAlert("COMMAND REFUSED", msg, ALERT_WARNING);
      return;
    }
  }

  /* 2. Controller events. Only the newest is inspected: the log keeps the
        rest, and one popup at a time is all an operator can acknowledge. */
  HmiEvent ev;
  if (hmiCopyEvents(&ev, 1) == 1 && ev.atMs != lastEventAtMs) {
    lastEventAtMs = ev.atMs;
    if (!alertIsOpen()) {
      switch (ev.type) {
        case EV_FAULT_TRIP:
          showAlert("SAFETY TRIP", ev.text, ALERT_ERROR);
          return;
        case EV_ENDPOINT_PENDING:
          showAlert("ENDPOINT REACHED",
                    "Open CONTROL and press CONFIRM ENDPOINT\nto start cooling.",
                    ALERT_INFO);
          return;
        default:
          break;  // stage changes etc. are shown on the dashboard
      }
    }
  }

  /* 3. Roast finished (cooling completed). Edge-triggered on the state. */
  if (connected && stateSerial != lastStateSerial) {
    lastStateSerial = stateSerial;
    if (st.roastState == ROAST_DONE && lastRoastState != ROAST_DONE &&
        !alertIsOpen()) {
      char msg[64];
      snprintf(msg, sizeof(msg), "Session time %lu min",
               (unsigned long)(st.sessionMs / 60000UL));
      showAlert("ROAST COMPLETE", msg, ALERT_SUCCESS);
    }
    lastRoastState = st.roastState;
  }

  /* 4. Link loss. The controller has already removed heater authority; the
        operator must not be looking at frozen numbers believing otherwise. */
  if (sawLink && !connected && !alertIsOpen()) {
    const uint32_t now = millis();
    if (lastLinkAlertMs == 0 || (now - lastLinkAlertMs) > 30000UL) {
      lastLinkAlertMs = now;
      showAlert("LINK LOST",
                "No telemetry from the controller.\nHeater authority is removed.",
                ALERT_ERROR);
    }
  }
}
