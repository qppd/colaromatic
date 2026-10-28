#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <lvgl.h>

enum ScreenId {
  SCREEN_BOOT,
  SCREEN_DASHBOARD,
  SCREEN_CONTROL,
  SCREEN_ANALYTICS,
  SCREEN_DIAGNOSTICS,
  SCREEN_COUNT
};

void screenManagerInit();
void loadScreen(ScreenId id);
ScreenId getCurrentScreen();
void updateCurrentScreen();
void backBtnCallback(lv_event_t *e);

#endif  // SCREEN_MANAGER_H
