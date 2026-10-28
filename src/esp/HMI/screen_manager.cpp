#include "screen_manager.h"

#include <Arduino.h>

#include "analytics_screen.h"
#include "boot_screen.h"
#include "control_screen.h"
#include "dashboard_screen.h"
#include "diagnostics_screen.h"
#include "lvgl_v8_port.h"
#include "ui_theme.h"

static ScreenId currentScreen = SCREEN_BOOT;
static ScreenId previousScreen = SCREEN_DASHBOARD;
static lv_obj_t *screens[SCREEN_COUNT] = {NULL};

static lv_obj_t *createScreen(ScreenId id) {
  switch (id) {
    case SCREEN_BOOT: return createBootScreen();
    case SCREEN_DASHBOARD: return createDashboardScreen();
    case SCREEN_CONTROL: return createControlScreen();
    case SCREEN_ANALYTICS: return createAnalyticsScreen();
    case SCREEN_DIAGNOSTICS: return createDiagnosticsScreen();
    default: return NULL;
  }
}

static const char *screenName(ScreenId id) {
  switch (id) {
    case SCREEN_BOOT: return "BOOT";
    case SCREEN_DASHBOARD: return "DASHBOARD";
    case SCREEN_CONTROL: return "CONTROL";
    case SCREEN_ANALYTICS: return "ANALYTICS";
    case SCREEN_DIAGNOSTICS: return "DIAGNOSTICS";
    default: return "UNKNOWN";
  }
}

void screenManagerInit() {
  lvgl_port_lock(-1);
  screens[SCREEN_BOOT] = createScreen(SCREEN_BOOT);
  if (screens[SCREEN_BOOT] != NULL) {
    lv_scr_load(screens[SCREEN_BOOT]);
    currentScreen = SCREEN_BOOT;
    Serial.println("[HMI] screen: BOOT");
  }
  lvgl_port_unlock();
}

void loadScreen(ScreenId id) {
  if (id == currentScreen || id >= SCREEN_COUNT) return;

  lvgl_port_lock(-1);

  previousScreen = currentScreen;

  if (screens[id] == NULL) {
    screens[id] = createScreen(id);
    if (screens[id] == NULL) {
      lvgl_port_unlock();
      return;
    }
  }

  /* auto_del: LVGL owns the old screen and frees it after the transition. The
     cached pointer is cleared below so it is rebuilt (with fresh widget
     pointers) the next time the screen is opened. */
  lv_scr_load_anim(screens[id], LV_SCR_LOAD_ANIM_FADE_ON, ANIM_SCREEN_TRANS, 0,
                   true);
  currentScreen = id;
  Serial.printf("[HMI] screen: %s\n", screenName(id));

  if (previousScreen < SCREEN_COUNT) screens[previousScreen] = NULL;

  lvgl_port_unlock();
}

ScreenId getCurrentScreen() { return currentScreen; }

void updateCurrentScreen() {
  lvgl_port_lock(-1);
  switch (currentScreen) {
    case SCREEN_BOOT: updateBootScreen(); break;
    case SCREEN_DASHBOARD: updateDashboardScreen(); break;
    case SCREEN_CONTROL: updateControlScreen(); break;
    case SCREEN_ANALYTICS: updateAnalyticsScreen(); break;
    case SCREEN_DIAGNOSTICS: updateDiagnosticsScreen(); break;
    default: break;  // the boot screen is static
  }
  lvgl_port_unlock();
}

void backBtnCallback(lv_event_t *e) {
  (void)e;
  loadScreen(SCREEN_DASHBOARD);
}
