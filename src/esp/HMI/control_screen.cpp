/*
 * control_screen.cpp — everything the operator sets before and during a roast.
 *
 *   profile      : the 12 bean/roast-level profiles the controller knows, by
 *                  index. Selection is refused by the controller mid-session,
 *                  and the buttons are disabled here for the same reason.
 *   mode         : MANUAL (fixed duty) or AUTOMATIC (colour -> setpoint -> PID).
 *                  The controller locks automatic entry once a roast is live.
 *   setpoint     : the automatic target in degC, sent as degC*10.
 *   manual duty  : 0..ceiling percent, manual mode only.
 *   fan          : on/off only — the fan SSR has no PWM.
 *   stage        : operator confirmation of an estimated stage.
 *   endpoint     : confirmation of a pending endpoint (starts cooling).
 *
 * Nothing here is trusted locally: every control sends a command and the
 * controller's answer (or its telemetry) is what changes the display.
 */

#include "control_screen.h"

#include <Arduino.h>
#include <stdio.h>

#include "alert_popup.h"
#include "esp_now_link.h"
#include "hmi_data.h"
#include "screen_manager.h"
#include "ui_styles.h"
#include "ui_theme.h"

/* Setpoint bounds for the automatic target. The colour loop only ever asks for
   COLOR_SETPOINT_MIN_C..MAX_C (150..230 degC); the manual setpoint is allowed
   a little wider so a commissioning run can test the edges. */
#define SP_MIN_C10 600
#define SP_MAX_C10 2400

static lv_obj_t *profileBtns[GEN_PROFILE_COUNT] = {NULL};
static lv_obj_t *modeAutoBtn = NULL;
static lv_obj_t *modeManualBtn = NULL;
static lv_obj_t *setpointLabel = NULL;
static lv_obj_t *currentLabel = NULL;
static lv_obj_t *dutyLabel = NULL;
static lv_obj_t *dutySlider = NULL;
static lv_obj_t *fanSwitch = NULL;
static lv_obj_t *stageDropdown = NULL;
static lv_obj_t *confirmStageBtn = NULL;
static lv_obj_t *endpointBtn = NULL;
static lv_obj_t *startBtn = NULL;
static lv_obj_t *coolBtn = NULL;
static lv_obj_t *sessionLabel = NULL;

static const char *STAGE_OPTIONS =
    "TURNING POINT\nCOLOUR CHANGE\n1ST CRACK\nMEDIUM ROAST\n2ND CRACK\nDARK ROAST\nDONE";

/* Dropdown order -> LinkStage. STAGE_CHARGE is absent on purpose: a session
   starts there, so it cannot be confirmed "forward" into. COLOUR CHANGE is
   offered because the controller cannot detect it (no validated colour trend)
   but an operator may still call it by eye — that is METHOD_OPERATOR.
   Keep this table and STAGE_OPTIONS in the same order. */
static const uint8_t STAGE_FOR_OPTION[] = {
    STAGE_TURNING_POINT, STAGE_COLOR_CHANGE, STAGE_FIRST_CRACK,
    STAGE_MEDIUM_ROAST, STAGE_SECOND_CRACK, STAGE_DARK_ROAST, STAGE_DONE};

static void setHidden(lv_obj_t *obj, bool hidden) {
  if (!lv_obj_is_valid(obj)) return;
  if (hidden) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

static bool sessionLive(const LinkState &st, bool connected) {
  if (!connected) return false;
  return st.roastState == ROAST_RUNNING || st.roastState == ROAST_HOLD ||
         st.roastState == ROAST_COOLING;
}

/* ---- callbacks ---------------------------------------------------------- */

static void profileCb(lv_event_t *e) {
  const uint8_t idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (idx >= GEN_PROFILE_COUNT) return;
  hmiSelectProfile(idx);
  if (!hmiCmdSelectProfile(idx)) {
    showAlert("LINK DOWN", "Could not send the profile: no controller link.",
              ALERT_ERROR);
  }
}

static void modeCb(lv_event_t *e) {
  const uint8_t mode = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  hmiSetMode(mode);
  hmiCmdSetMode(mode);
}

static void setpointStep(int deltaC10) {
  uint8_t profile, mode, duty;
  int16_t c10;
  hmiOperatorSnapshot(profile, mode, c10, duty);
  int32_t next = (int32_t)c10 + deltaC10;
  if (next < SP_MIN_C10) next = SP_MIN_C10;
  if (next > SP_MAX_C10) next = SP_MAX_C10;
  hmiSetSetpointC10((int16_t)next);
  hmiCmdSetSetpointC10((int16_t)next);
}

static void spMinus10Cb(lv_event_t *e) { (void)e; setpointStep(-100); }
static void spMinus1Cb(lv_event_t *e) { (void)e; setpointStep(-10); }
static void spPlus1Cb(lv_event_t *e) { (void)e; setpointStep(+10); }
static void spPlus10Cb(lv_event_t *e) { (void)e; setpointStep(+100); }

static void dutyCb(lv_event_t *e) {
  (void)e;
  if (!lv_obj_is_valid(dutySlider)) return;
  const uint8_t pct = (uint8_t)lv_slider_get_value(dutySlider);
  hmiSetManualDuty(pct);
  hmiCmdSetDuty(pct);
}

static void fanCb(lv_event_t *e) {
  (void)e;
  if (!lv_obj_is_valid(fanSwitch)) return;
  const bool on = lv_obj_has_state(fanSwitch, LV_STATE_CHECKED);
  linkSendCmd(OP_SET_FAN, on ? 100 : 0, 0);
}

static void confirmStageCb(lv_event_t *e) {
  (void)e;
  if (!lv_obj_is_valid(stageDropdown)) return;
  const uint16_t sel = lv_dropdown_get_selected(stageDropdown);
  if (sel >= sizeof(STAGE_FOR_OPTION) / sizeof(STAGE_FOR_OPTION[0])) return;
  hmiCmdConfirmStage(STAGE_FOR_OPTION[sel]);
}

static void confirmEndpointCb(lv_event_t *e) {
  (void)e;
  hmiCmdConfirmEndpoint();
}

static void startCb(lv_event_t *e) {
  (void)e;
  uint8_t profile, mode, duty;
  int16_t c10;
  hmiOperatorSnapshot(profile, mode, c10, duty);
  if (profile == LINK_PROFILE_NONE) {
    showAlert("NO PROFILE", "Select a bean profile first.", ALERT_WARNING);
    return;
  }
  if (!hmiCmdStart()) {
    showAlert("LINK DOWN", "Could not send START: no controller link.",
              ALERT_ERROR);
  }
}

static void coolCb(lv_event_t *e) { (void)e; hmiCmdCool(); }

/* ---- widget builders ---------------------------------------------------- */

static void createProfileGrid(lv_obj_t *parent) {
  lv_obj_t *title = lv_label_create(parent);
  lv_label_set_text(title, "BEAN PROFILE");
  lv_obj_add_style(title, &style_text_label, 0);

  lv_obj_t *grid = lv_obj_create(parent);
  lv_obj_set_size(grid, LV_PCT(100), 170);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(grid, 4, 0);
  lv_obj_set_style_pad_column(grid, 4, 0);

  for (uint8_t i = 0; i < GEN_PROFILE_COUNT; i++) {
    lv_obj_t *btn = createButton(grid, GEN_PROFILES[i].label, 160, 34,
                                 &style_btn_nav);
    /* The button style carries the 20 px font; the label is a child, so the
       font has to be set on it to keep "Arabica / Medium" inside 160 px. */
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label != NULL) lv_obj_set_style_text_font(label, FONT_SMALL, 0);
    lv_obj_set_user_data(btn, (void *)(intptr_t)i);
    lv_obj_add_event_cb(btn, profileCb, LV_EVENT_CLICKED, NULL);
    profileBtns[i] = btn;
  }
}

static lv_obj_t *createSwitchRow(lv_obj_t *parent, const char *label,
                                 lv_event_cb_t cb) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), 40);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *lbl = lv_label_create(row);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(lbl, COLOR_TEXT_PRIMARY, 0);

  lv_obj_t *sw = lv_switch_create(row);
  lv_obj_set_size(sw, 56, 28);
  lv_obj_set_style_bg_color(sw, COLOR_BG_BUTTON, 0);
  lv_obj_set_style_bg_color(sw, COLOR_SUCCESS, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
  return sw;
}

lv_obj_t *createControlScreen() {
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_add_style(scr, &style_screen_bg, 0);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  createTopBar(scr, "ROAST CONTROL", true);

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

  /* ---------------- left card: profile ---------------- */
  lv_obj_t *leftCard = createCard(content, 370, 400);
  lv_obj_set_flex_flow(leftCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(leftCard, 6, 0);
  createProfileGrid(leftCard);

  currentLabel = lv_label_create(leftCard);
  lv_label_set_text(currentLabel, "current: --");
  lv_obj_set_style_text_font(currentLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(currentLabel, COLOR_TEXT_SECONDARY, 0);

  createDivider(leftCard);

  lv_obj_t *stageTitle = lv_label_create(leftCard);
  lv_label_set_text(stageTitle, "CONFIRM STAGE");
  lv_obj_add_style(stageTitle, &style_text_label, 0);

  lv_obj_t *stageRow = lv_obj_create(leftCard);
  lv_obj_set_size(stageRow, LV_PCT(100), 44);
  lv_obj_set_style_bg_opa(stageRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(stageRow, 0, 0);
  lv_obj_set_style_pad_all(stageRow, 0, 0);
  lv_obj_set_scrollbar_mode(stageRow, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(stageRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(stageRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  stageDropdown = lv_dropdown_create(stageRow);
  lv_dropdown_set_options(stageDropdown, STAGE_OPTIONS);
  lv_obj_set_width(stageDropdown, 190);
  lv_obj_set_style_text_font(stageDropdown, FONT_NORMAL, 0);
  lv_obj_set_style_bg_color(stageDropdown, COLOR_BG_BUTTON, 0);
  lv_obj_set_style_text_color(stageDropdown, COLOR_TEXT_PRIMARY, 0);

  confirmStageBtn = createButton(stageRow, "CONFIRM", 130, 40, &style_btn_nav);
  lv_obj_add_event_cb(confirmStageBtn, confirmStageCb, LV_EVENT_CLICKED, NULL);

  sessionLabel = lv_label_create(leftCard);
  lv_label_set_text(sessionLabel, "stage: --");
  lv_obj_set_style_text_font(sessionLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(sessionLabel, COLOR_TEXT_SECONDARY, 0);

  /* Only shown while the controller reports a pending endpoint. */
  endpointBtn = createButton(leftCard,
                             LV_SYMBOL_OK "  CONFIRM ENDPOINT -> COOL",
                             LV_PCT(100), 40, &style_btn_primary);
  lv_obj_add_event_cb(endpointBtn, confirmEndpointCb, LV_EVENT_CLICKED, NULL);
  setHidden(endpointBtn, true);

  /* ---------------- right card: mode, targets, actions ---------------- */
  lv_obj_t *rightCard = createCard(content, 370, 400);
  lv_obj_set_flex_flow(rightCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(rightCard, 6, 0);

  lv_obj_t *modeTitle = lv_label_create(rightCard);
  lv_label_set_text(modeTitle, "CONTROL MODE");
  lv_obj_add_style(modeTitle, &style_text_label, 0);

  lv_obj_t *modeRow = lv_obj_create(rightCard);
  lv_obj_set_size(modeRow, LV_PCT(100), 46);
  lv_obj_set_style_bg_opa(modeRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(modeRow, 0, 0);
  lv_obj_set_style_pad_all(modeRow, 0, 0);
  lv_obj_set_scrollbar_mode(modeRow, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(modeRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(modeRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  modeManualBtn = createButton(modeRow, "MANUAL", 165, 42, &style_btn_nav);
  lv_obj_set_user_data(modeManualBtn, (void *)(intptr_t)MODE_MANUAL);
  lv_obj_add_event_cb(modeManualBtn, modeCb, LV_EVENT_CLICKED, NULL);

  modeAutoBtn = createButton(modeRow, "AUTOMATIC", 165, 42, &style_btn_nav);
  lv_obj_set_user_data(modeAutoBtn, (void *)(intptr_t)MODE_AUTO);
  lv_obj_add_event_cb(modeAutoBtn, modeCb, LV_EVENT_CLICKED, NULL);

  createDivider(rightCard);

  lv_obj_t *spTitle = lv_label_create(rightCard);
  lv_label_set_text(spTitle, "AUTOMATIC TARGET");
  lv_obj_add_style(spTitle, &style_text_label, 0);

  lv_obj_t *spRow = lv_obj_create(rightCard);
  lv_obj_set_size(spRow, LV_PCT(100), 48);
  lv_obj_set_style_bg_opa(spRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(spRow, 0, 0);
  lv_obj_set_style_pad_all(spRow, 0, 0);
  lv_obj_set_scrollbar_mode(spRow, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(spRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(spRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *spDown10 = createButton(spRow, "-10", 58, 42, &style_btn_nav);
  lv_obj_add_event_cb(spDown10, spMinus10Cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *spDown1 = createButton(spRow, "-1", 58, 42, &style_btn_nav);
  lv_obj_add_event_cb(spDown1, spMinus1Cb, LV_EVENT_CLICKED, NULL);

  setpointLabel = lv_label_create(spRow);
  lv_label_set_text(setpointLabel, "-- \xC2\xB0" "C");
  lv_obj_set_style_text_font(setpointLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(setpointLabel, COLOR_ACCENT, 0);

  lv_obj_t *spUp1 = createButton(spRow, "+1", 58, 42, &style_btn_nav);
  lv_obj_add_event_cb(spUp1, spPlus1Cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *spUp10 = createButton(spRow, "+10", 58, 42, &style_btn_nav);
  lv_obj_add_event_cb(spUp10, spPlus10Cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *dutyTitle = lv_label_create(rightCard);
  lv_label_set_text(dutyTitle, "MANUAL DUTY");
  lv_obj_add_style(dutyTitle, &style_text_label, 0);

  lv_obj_t *dutyRow = lv_obj_create(rightCard);
  lv_obj_set_size(dutyRow, LV_PCT(100), 52);
  lv_obj_set_style_bg_opa(dutyRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dutyRow, 0, 0);
  lv_obj_set_style_pad_all(dutyRow, 0, 0);
  lv_obj_set_scrollbar_mode(dutyRow, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(dutyRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dutyRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  dutySlider = lv_slider_create(dutyRow);
  lv_obj_set_width(dutySlider, 220);
  lv_slider_set_range(dutySlider, 0, HMI_DUTY_CEILING_PCT);
  lv_slider_set_value(dutySlider, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(dutySlider, COLOR_BG_BUTTON, 0);
  lv_obj_set_style_bg_color(dutySlider, COLOR_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(dutySlider, COLOR_ACCENT, LV_PART_KNOB);
  lv_obj_set_style_pad_all(dutySlider, 6, LV_PART_KNOB);
  lv_obj_add_event_cb(dutySlider, dutyCb, LV_EVENT_VALUE_CHANGED, NULL);

  dutyLabel = lv_label_create(dutyRow);
  lv_label_set_text_fmt(dutyLabel, "%u %%", (unsigned)0);
  lv_obj_set_style_text_font(dutyLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(dutyLabel, COLOR_TEXT_PRIMARY, 0);

  fanSwitch = createSwitchRow(rightCard, LV_SYMBOL_REFRESH "  FAN (on/off)",
                              fanCb);

  createDivider(rightCard);

  lv_obj_t *btnRow = lv_obj_create(rightCard);
  lv_obj_set_size(btnRow, LV_PCT(100), 52);
  lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btnRow, 0, 0);
  lv_obj_set_style_pad_all(btnRow, 0, 0);
  lv_obj_set_scrollbar_mode(btnRow, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  startBtn = createButton(btnRow, LV_SYMBOL_PLAY " START", 165, 48,
                          &style_btn_success);
  lv_obj_add_event_cb(startBtn, startCb, LV_EVENT_CLICKED, NULL);

  coolBtn = createButton(btnRow, LV_SYMBOL_STOP " STOP & COOL", 165, 48,
                         &style_btn_primary);
  lv_obj_add_event_cb(coolBtn, coolCb, LV_EVENT_CLICKED, NULL);

  return scr;
}

void updateControlScreen() {
  if (!lv_obj_is_valid(setpointLabel)) return;

  LinkState st;
  bool connected = false;
  uint8_t serial = 0;
  hmiSnapshot(st, connected, serial);
  (void)serial;  // this screen has no chart sampling

  uint8_t profile, mode, dutySel;
  int16_t setpointC10;
  hmiOperatorSnapshot(profile, mode, setpointC10, dutySel);

  const bool live = sessionLive(st, connected);

  /* ---- profile grid highlight + lock ---- */
  const uint8_t activeProfile =
      (live && st.profileIdx < GEN_PROFILE_COUNT) ? st.profileIdx : profile;
  for (uint8_t i = 0; i < GEN_PROFILE_COUNT; i++) {
    lv_obj_t *btn = profileBtns[i];
    if (!lv_obj_is_valid(btn)) continue;
    const bool selected = (i == activeProfile);
    lv_obj_set_style_bg_color(btn, selected ? COLOR_ACCENT : COLOR_BG_BUTTON, 0);
    /* A live session fixes the profile on the controller, so do not offer the
       edit here either. */
    if (live) {
      lv_obj_add_state(btn, LV_STATE_DISABLED);
    } else {
      lv_obj_clear_state(btn, LV_STATE_DISABLED);
    }
  }
  if (lv_obj_is_valid(currentLabel)) {
    if (activeProfile < GEN_PROFILE_COUNT) {
      lv_label_set_text_fmt(currentLabel, "current: %s (%s)",
                            GEN_PROFILES[activeProfile].id, hmiModeText(mode));
    } else {
      lv_label_set_text(currentLabel, "current: none");
    }
  }

  /* ---- mode ---- */
  if (lv_obj_is_valid(modeAutoBtn)) {
    lv_obj_set_style_bg_color(modeAutoBtn,
                              (mode == MODE_AUTO) ? COLOR_ACCENT
                                                  : COLOR_BG_BUTTON,
                              0);
  }
  if (lv_obj_is_valid(modeManualBtn)) {
    lv_obj_set_style_bg_color(modeManualBtn,
                              (mode == MODE_MANUAL) ? COLOR_ACCENT
                                                    : COLOR_BG_BUTTON,
                              0);
  }

  /* ---- setpoint (degC*10 -> degC) ---- */
  if (lv_obj_is_valid(setpointLabel)) {
    lv_label_set_text_fmt(setpointLabel, "%.1f \xC2\xB0" "C",
                          (double)(setpointC10 / 10.0f));
  }

  /* ---- manual duty ---- */
  if (lv_obj_is_valid(dutyLabel)) {
    lv_label_set_text_fmt(dutyLabel, "%u %%", (unsigned)dutySel);
  }
  if (lv_obj_is_valid(dutySlider) &&
      !lv_obj_has_state(dutySlider, LV_STATE_PRESSED)) {
    lv_slider_set_value(dutySlider, (int)dutySel, LV_ANIM_OFF);
  }

  /* ---- fan (controller state, not the switch's memory) ---- */
  if (lv_obj_is_valid(fanSwitch) && !lv_obj_has_state(fanSwitch, LV_STATE_PRESSED)) {
    const bool on = connected && st.fanPct > 0;
    if (on) {
      lv_obj_add_state(fanSwitch, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(fanSwitch, LV_STATE_CHECKED);
    }
  }

  /* ---- stage / session ---- */
  if (lv_obj_is_valid(sessionLabel)) {
    if (connected) {
      lv_label_set_text_fmt(sessionLabel, "stage: %s  \xC2\xB7  %s",
                            hmiStageText(st.stage),
                            hmiStageMethodText(st.stageMethod));
    } else {
      lv_label_set_text(sessionLabel, "stage: no link");
    }
  }

  /* ---- endpoint + action buttons ---- */
  setHidden(endpointBtn, !(connected && st.endpointPending));
  setHidden(startBtn, live);
  setHidden(coolBtn, !(connected && (st.roastState == ROAST_RUNNING ||
                                     st.roastState == ROAST_HOLD)));

  /* Fan may only be driven when no session owns the actuators — the
     controller refuses OP_SET_FAN otherwise (ACK_STATE SESSION_ACTIVE). */
  if (lv_obj_is_valid(fanSwitch)) {
    if (live || !connected) {
      lv_obj_add_state(fanSwitch, LV_STATE_DISABLED);
    } else {
      lv_obj_clear_state(fanSwitch, LV_STATE_DISABLED);
    }
  }
}
