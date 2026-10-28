/*
 * ColAromatic — HMI touch panel firmware (PROVISIONAL)
 *
 * ESP32-S3-Touch-LCD-4.3 panel. Renders the operator interface and talks to
 * the machine controller (src/esp/ColAromatic) over ESP-NOW, addressed by MAC.
 * There is no PC in the machine and no network: the panel and the controller
 * are the whole system.
 *
 *   HMI.ino              this file — setup()/loop() orchestration
 *   esp_now_link.*       ESP-NOW transport, pairing, beacons (LinkFrames.h)
 *   hmi_data.*           the panel's locked copy of the controller telemetry
 *   screen_manager.*     screen lifecycle and navigation
 *   boot_screen.*        splash with both MACs and the link state
 *   dashboard_screen.*   live roast: temperature, RoR, duty, stage, actions
 *   control_screen.*     profile, mode, setpoint, manual duty, fan, stage
 *   analytics_screen.*   temperature/target, RoR, colour and gas curves
 *   diagnostics_screen.* link, inputs, faults, events, re-pair, fault reset
 *   alert_popup.*        safety trips, refusals, endpoint, completion
 *   ui_theme.h           geometry, palette, fonts
 *   ui_styles.*          shared LVGL styles and widget helpers
 *
 * THE PANEL IS NOT IN THE CONTROL PATH. It sends operator commands; the
 * controller's RoastControl owns the roast and HeaterSafety owns heater
 * authority. If the panel dies, or its beacons stop, the controller removes
 * heater power on its own (LINK_TIMEOUT_MS). Nothing displayed here is a
 * substitute for the machine's physical E-stop and thermal cutoff.
 *
 * SETUP
 *   1. Flash the controller. Its serial log prints "controller MAC ...".
 *   2. Paste that MAC into CONTROLLER_PEER_MAC in esp_now_link.h (or leave it
 *      zero and use DIAGNOSTICS -> RE-PAIR), then flash this panel.
 *   3. Paste this panel's MAC (shown on the boot screen and in DIAGNOSTICS)
 *      into HMI_PEER_MAC in the controller's CONF.h and flash it.
 *   Both firmwares must use the same LINK_WIFI_CHANNEL (LinkFrames.h).
 *
 * The board support files (lvgl_v8_port.*, esp_panel_board_custom_conf.h) are
 * taken from the reference MultiDryerHMI sketch, which ran on the same
 * Espressif esp_panel stack. The board configuration must match the panel
 * actually fitted; see the note at the top of esp_panel_board_custom_conf.h.
 */

#include <Arduino.h>
#include <esp_mac.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>

#include "alert_popup.h"
#include "hmi_data.h"
#include "esp_now_link.h"
#include "lvgl_v8_port.h"
#include "screen_manager.h"
#include "ui_styles.h"
#include "ui_theme.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

static Board *s_board = nullptr;

static void uiUpdateTimerCb(lv_timer_t *timer) {
  (void)timer;
  updateCurrentScreen();
  checkAlerts();
}

void setup() {
  Serial.begin(115200);
  delay(200);  // let the USB-CDC port settle before the first log lines
  Serial.println("=== ColAromatic HMI ===");

  /* Print the station MAC before anything that can fail: it is the value that
     has to be pasted into the controller's HMI_PEER_MAC. */
  {
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
      char text[24];
      linkMacText(mac, text, sizeof(text));
      Serial.printf("[HMI] MAC (station): %s\n", text);
    }
  }

  hmiInit();

  /* Radio first: the panel can then pair while the display is still coming up,
     and a display failure does not leave a daemon-less board. */
  linkInit();

  Serial.println("[HMI] initializing display board...");
  s_board = new Board();
  s_board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
  auto lcd = s_board->getLCD();
  lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
  auto lcd_bus = lcd->getBus();
  if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
    static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(
        lcd->getFrameWidth() * 10);
  }
#endif
#endif
  assert(s_board->begin());

  Serial.println("[HMI] initializing LVGL...");
  if (!lvgl_port_init(s_board->getLCD(), s_board->getTouch())) {
    Serial.println("[HMI] LVGL port init FAILED — panel unusable");
  }

  Serial.println("[HMI] building UI...");
  lvgl_port_lock(-1);

  initStyles();
  alertInit();
  screenManagerInit();

  /* The controller sends telemetry every 500 ms; UI_TICK_MS is the redraw
     cadence, not a second data rate. */
  lv_timer_create(uiUpdateTimerCb, UI_TICK_MS, NULL);

  lvgl_port_unlock();

  Serial.printf("[HMI] ready, free heap %lu KB\n",
                (unsigned long)(ESP.getFreeHeap() / 1024));
}

void loop() {
  /* Beacons, the link timeout and any pending re-pair. Everything drawn on the
     screen is fed from the frames this receives. */
  linkUpdate(millis());
  delay(5);
}
