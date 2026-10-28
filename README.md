# ColAromatic — ESP32 Controller Firmware

ESP32 38-pin controller firmware for the ColAromatic coffee roasting machine.
Uses LVGL for HMI touch panel display, controls heating, color sensing,
and communicates with the HMI panel via ESP-NOW.

## Hardware

- **Controller**: ESP32 38-pin dev board
- **HMI Panel**: ESP32-S3-Touch-LCD-4.3 (touch screen)
- **Sensors**: MAX6675 (K-type thermocouple), TCS3200 (color), BME688 (gas), envelope microphone

## Repository Layout

```
src/esp/ColAromatic/   Main controller firmware (ESP32 38-pin)
  ColAromatic.ino      Arduino sketch entry point
  PINS.h               GPIO pin assignments (single source of truth)
  CONF.h               Firmware timing/threshold constants
  GenConfig.h          timing/threshold table (formerly generated; now hand-maintained)
  GenFuzzy.h           fuzzy rule table (formerly generated; now hand-maintained)
  GenProfiles.h        12 roast profiles (formerly generated; now hand-maintained)
  HeaterSafety.*       heater/fan SSR outputs, E-stop, watchdog, latches
  RoastControl.*       Session states, RoR, stages, duty shaping
  Fuzzy.*              Mamdani fuzzy inference engine
  HmiLink.*            ESP-NOW link to HMI touch panel
  HostLink.*           USB serial service protocol (bench/debug)
  ThermocoupleSensor.* MAX6675 K-type probe driver
  ColorSensor.*        TCS3200 light-to-frequency color sensor
  EnoseSensor.*        BME688 gas sensor
  AcousticSensor.*     Envelope microphone / crack detection
src/esp/HMI/           HMI touch panel firmware (ESP32-S3, LVGL 8)
  HMI.ino              Arduino sketch entry point
  esp_now_link.*       ESP-NOW transport + pairing (LinkFrames.h)
  hmi_data.*           locked copy of the controller telemetry
  screen_manager.*     boot / dashboard / control / curves / diagnostics
  * _screen.*          the five screens
  alert_popup.*        trips, refusals, endpoint, completion  ui_theme.h, ui_styles.*  geometry, palette, fonts, widgets

docs/                  Technical documentation (01–17, see the table below)
```

## Build & Flash

**Controller.** Open `src/esp/ColAromatic/ColAromatic.ino` in Arduino IDE.
Select `ESP32 Dev Module` as the board. Install required libraries:
- `EspNow`
- `Adafruit_BME688`
- no thermocouple library: the MAX6675 is read directly over SPI

**Panel.** Open `src/esp/HMI/HMI.ino`, select the ESP32-S3 board that matches the
panel, and install `lvgl` (v8) plus the `esp_panel` board support. `lv_conf.h`
must enable Montserrat 16/20/24/30/36/48.

> **Open (defect G6):** `src/esp/HMI/esp_panel_board_custom_conf.h` is still the
> reference project's **Waveshare ESP32-S3-Touch-LCD-7** configuration (800×480,
> ST7262), while this README lists the fitted panel as the 4.3″ model. Confirm
> the actual panel and set `ESP_PANEL_BOARD_NAME` accordingly before the first
> flash, or the display will not initialise.

**Pairing by MAC.** Each board prints its station MAC at boot. Paste the
controller's MAC into `CONTROLLER_PEER_MAC` (`src/esp/HMI/esp_now_link.h`) and
the panel's into `HMI_PEER_MAC` (`src/esp/ColAromatic/CONF.h`). Both may be left
zero: the panel then pairs with the first controller that answers its pairing
beacon, and `DIAGNOSTICS → RE-PAIR` repeats that handshake.

See [docs/17_firmware_build_flash.md](docs/17_firmware_build_flash.md) for details.

## Pin Map

All GPIO assignments are in [`src/esp/ColAromatic/PINS.h`](src/esp/ColAromatic/PINS.h).

| Component | Pins |
|---|---|
| MAX6675 (thermocouple) | SCK=18, SO=19, CS=5 |
| TCS3200 (color sensor) | S0=32, S1=33, S2=27, S3=14, OUT=34, LED=25 |
| BME688 (gas sensor) | SDA=21, SCL=22 |
| Heater SSR | 26, 13 (optional second channel) |
| Fan SSR | 23 (on/off only, no PWM; GPIO 4 is a strapping pin and stays free) |
| E-stop | GPIO=16 (active low, internal pull-up) |
| Status LED | GPIO=2 |
| Microphone | ADC1_CH0=36 |

## Communication Protocol

- **HMI ↔ Controller**: ESP-NOW unicast (no AP, no internet, offline)
- **Bench/Debug**: USB serial ASCII protocol (115200 baud)

See [docs/05_serial_protocol.md](docs/05_serial_protocol.md) for frame formats.

## Safety

- This firmware is NOT the independent protection for the heater circuit.
  A separately wired thermal cutoff is required.
- Every interlock evaluates on every loop pass: invalid thermocouple, stale
  reading, E-stop, over-temperature, RoR runaway, or link loss removes heater power.
- The heater and the fan each have their own SSR. Heater duty uses
  time-proportioning; the fan SSR is on/off only (no PWM), so a non-zero fan
  command runs at full speed. Phase-angle SSRs must NOT be driven this way.

## Status

PROVISIONAL FIRMWARE. Thresholds are engineering placeholders requiring
calibration on real hardware. No heating element has been energized in testing.

## Documentation

| Document | Contents |
|---|---|
| [01 Requirements & traceability](docs/01_requirements_traceability.md) | requirements register |
| [02 Architecture](docs/02_architecture.md) | system layers, data flow |
| [03 Hardware BOM & gap analysis](docs/03_hardware_bom_gap_analysis.md) | component list |
| [04 Wiring & pin map](docs/04_wiring_and_pinmap.md) | detailed pin assignments |
| [05 Serial protocol](docs/05_serial_protocol.md) | frame formats, commands |
| [06 Fuzzy logic design](docs/06_fuzzy_design.md) | inference method, rules |
| [07 Color calibration](docs/07_color_calibration.md) | measurement workflow |
| [08 Data schema](docs/08_data_schema.md) | frame, enum and table schemas |
| [09 Test plan](docs/09_test_plan.md) | static checks, tests to write, commissioning record |
| [10 Experimental protocol](docs/10_experimental_protocol.md) | trial design |
| [11 User manual](docs/11_user_manual.md) | operator workflow |
| [12 Troubleshooting](docs/12_troubleshooting.md) | symptoms & fixes |
| [13 Known limitations](docs/13_known_limitations.md) | unvalidated assumptions |
| [14 Handover checklist](docs/14_handover_checklist.md) | deliverables |
| [15 Risk register](docs/15_risk_register.md) | hazards & mitigations |
| [16 Project plan](docs/16_project_plan.md) | phases, gates, staged fallback |
| [17 Firmware build & flash](docs/17_firmware_build_flash.md) | Arduino IDE setup |
