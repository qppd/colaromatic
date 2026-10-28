#include "boot_screen.h"

#include <Arduino.h>
#include <stdio.h>

#include "esp_now_link.h"
#include "hmi_data.h"
#include "screen_manager.h"
#include "ui_styles.h"
#include "ui_theme.h"

static lv_obj_t *statusLabel = NULL;
static lv_obj_t *peerLabel = NULL;
static uint32_t bootStartMs = 0;
static bool leaving = false;

#define BOOT_MIN_SHOW_MS 2500
#define BOOT_MAX_SHOW_MS 10000

lv_obj_t *createBootScreen() {
  bootStartMs = millis();
  leaving = false;

  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_add_style(scr, &style_screen_bg, 0);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, HMI_TITLE);
  lv_obj_set_style_text_font(title, FONT_HUGE, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT_PRIMARY, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -80);

  lv_obj_t *subtitle = lv_label_create(scr);
  lv_label_set_text(subtitle, HMI_SUBTITLE);
  lv_obj_set_style_text_font(subtitle, FONT_MEDIUM, 0);
  lv_obj_set_style_text_color(subtitle, COLOR_TEXT_SECONDARY, 0);
  lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, -30);

  /* The two MACs are the pairing instructions: one goes into the controller's
     HMI_PEER_MAC, the other must appear here for the panel to talk to it. */
  uint8_t self[6];
  linkSelfMac(self);
  char selfText[24];
  linkMacText(self, selfText, sizeof(selfText));

  lv_obj_t *selfLabel = lv_label_create(scr);
  lv_label_set_text_fmt(selfLabel, "panel MAC  %s", selfText);
  lv_obj_set_style_text_font(selfLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(selfLabel, COLOR_TEXT_DIM, 0);
  lv_obj_align(selfLabel, LV_ALIGN_CENTER, 0, 20);

  char peerText[24];
  linkMacText(linkPeerMac(), peerText, sizeof(peerText));
  peerLabel = lv_label_create(scr);
  lv_label_set_text_fmt(peerLabel, "controller  %s", peerText);
  lv_obj_set_style_text_font(peerLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(peerLabel, COLOR_TEXT_DIM, 0);
  lv_obj_align(peerLabel, LV_ALIGN_CENTER, 0, 46);

  statusLabel = lv_label_create(scr);
  lv_label_set_text(statusLabel, "waiting for the controller...");
  lv_obj_set_style_text_font(statusLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(statusLabel, COLOR_WARNING, 0);
  lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 100);

  lv_obj_t *version = lv_label_create(scr);
  lv_label_set_text(version, HMI_VERSION_STRING);
  lv_obj_set_style_text_font(version, FONT_SMALL, 0);
  lv_obj_set_style_text_color(version, COLOR_TEXT_DIM, 0);
  lv_obj_align(version, LV_ALIGN_BOTTOM_MID, 0, -15);

  return scr;
}

void updateBootScreen() {
  if (leaving) return;

  LinkState st;
  bool connected = false;
  uint8_t serial = 0;
  hmiSnapshot(st, connected, serial);

  const uint32_t elapsed = millis() - bootStartMs;
  if (lv_obj_is_valid(statusLabel)) {
    if (connected) {
      lv_label_set_text(statusLabel, "controller connected");
      lv_obj_set_style_text_color(statusLabel, COLOR_SUCCESS, 0);
    } else {
      lv_label_set_text(statusLabel, "waiting for the controller...");
    }
  }
  if (lv_obj_is_valid(peerLabel)) {
    char peerText[24];
    linkMacText(linkPeerMac(), peerText, sizeof(peerText));
    lv_label_set_text_fmt(peerLabel, "controller  %s", peerText);
  }

  /* Leave as soon as the link is up (after the splash has been readable), or
     after BOOT_MAX_SHOW_MS so an unpaired panel still reaches the UI and can
     be paired from DIAGNOSTICS instead of locking up on the splash. */
  const bool ready = connected && elapsed >= BOOT_MIN_SHOW_MS;
  if (ready || elapsed >= BOOT_MAX_SHOW_MS) {
    leaving = true;
    loadScreen(SCREEN_DASHBOARD);
  }
}
