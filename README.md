# ColAromatic — ESP32 Controller Firmware

ESP32 38-pin controller firmware for the ColAromatic coffee roasting machine.
Uses LVGL for HMI touch panel display, controls heating, color sensing,
and communicates with the HMI panel via ESP-NOW.

## Hardware

- **Controller**: ESP32 38-pin dev board
- **HMI Panel**: ESP32-S3-Touch-LCD-4.3 (touch screen)
- **Sensors**: MAX31856 (K-type thermocouple), TCS3200 (color), BME688 (gas), envelope microphone

## Repository Layout

```
src/esp/ColAromatic/   Main controller firmware (ESP32 38-pin)
  ColAromatic.ino      Arduino sketch entry point
  PINS.h               GPIO pin assignments (single source of truth)
  CONF.h               Firmware timing/threshold constants
  GenConfig.h          Generated from system config (do not edit)
  GenFuzzy.h           Generated fuzzy rules (do not edit)
  GenProfiles.h        Generated roast profiles (do not edit)
  HeaterSafety.*       SSR/fan control, E-stop, watchdog, latches
  RoastControl.*       Session states, RoR, stages, duty shaping
  Fuzzy.*              Mamdani fuzzy inference engine
  HmiLink.*            ESP-NOW link to HMI touch panel
  HostLink.*           USB serial service protocol (bench/debug)
  ThermocoupleSensor.* MAX31856 K-type probe driver
  ColorSensor.*        TCS3200 light-to-frequency color sensor
  EnoseSensor.*        BME688 gas sensor
  AcousticSensor.*     Envelope microphone / crack detection
src/esp/HMI/           HMI touch panel firmware (ESP32-S3)
src/esp/common/        Shared frame definitions (LinkFrames.h)
docs/                  Technical documentation
```

## Build & Flash

Open `src/esp/ColAromatic/ColAromatic.ino` in Arduino IDE.
Select `ESP32 Dev Module` as the board. Install required libraries:
- `EspNow`
- `Adafruit_BME688`
- `MAX31856` (or equivalent)

See [docs/17_firmware_build_flash.md](docs/17_firmware_build_flash.md) for details.

## Pin Map

All GPIO assignments are in [`src/esp/ColAromatic/PINS.h`](src/esp/ColAromatic/PINS.h).

| Component | Pins |
|---|---|
| MAX31856 (thermocouple) | SCK=18, MISO=19, MOSI=23, CS=5 |
| TCS3200 (color sensor) | S0=32, S1=33, S2=27, S3=14, OUT=34, LED=25 |
| BME688 (gas sensor) | SDA=21, SCL=22 |
| Heater SSR | CH1=26, CH2=13 |
| Fan | PWM=4 |
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
- SSR switching uses time-proportioning; phase-angle SSRs must NOT be driven this way.

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
| [10 Experimental protocol](docs/10_experimental_protocol.md) | trial design |
| [11 User manual](docs/11_user_manual.md) | operator workflow |
| [12 Troubleshooting](docs/12_troubleshooting.md) | symptoms & fixes |
| [13 Known limitations](docs/13_known_limitations.md) | unvalidated assumptions |
| [14 Handover checklist](docs/14_handover_checklist.md) | deliverables |
| [15 Risk register](docs/15_risk_register.md) | hazards & mitigations |
| [17 Firmware build & flash](docs/17_firmware_build_flash.md) | Arduino IDE setup |
