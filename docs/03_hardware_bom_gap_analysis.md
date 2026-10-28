# 03 — Hardware inventory, compatibility audit and BOM gap analysis

## 0. What was actually available to audit

The repository as handed over contained a single file:
`contract/Quezon-Province-Programmer-Contract.docx` — a generic letter of
agreement with **blank** fee fields, no itemised quotation, no component
specifications, no quantities and no electrical ratings.

```
$ find . -maxdepth 3
contract/Quezon-Province-Programmer-Contract.docx
```

Consequences for this phase (Phase 1 of the master plan):

* The item list below is the **baseline from the project requirements**, not a
  verified quotation. Quantities, part numbers, voltage/current ratings and
  included accessories are **UNVERIFIED**.
* No datasheet-derived decision has been made anywhere in this repository. Every
  electrical statement is a question until the real parts are inspected.
* The firmware was therefore written with every GPIO in one header (`PINS.h`)
  and every threshold in `CONF.h` / `GenConfig.h`, so the board, the pin map and
  the limits can change without touching the control or safety logic.

**Action required before hardware work:** attach the actual quotation,
purchase receipts and photographs of the delivered parts. Then complete the
verification column below.

## 1. Baseline inventory and verification status

| # | Item (from the requirements) | Assumed role | Must verify | Status |
|---|---|---|---|---|
| 1 | ESP32 development board | main controller | exact module, flash/RAM, free GPIOs, USB chip, logic level, whether a second UART/SPI is needed for the HMI | UNVERIFIED |
| 2 | TCS3200 colour sensor | RGB / light-to-frequency | module vs bare chip (OUT drive capability), lens, illumination, mounting, distance, shielding | UNVERIFIED |
| 3 | K-type thermocouple | temperature | probe diameter/length, sheath material, grounded/ungrounded junction, response time, max temperature, connector type | UNVERIFIED |
| 4 | MAX6675 (or equivalent) interface | thermocouple front end | whether one is actually in the quotation, board logic level (**many MAX6675 breakouts are 5 V — level-shift SO/SCK/CS for the ESP32**), cold-junction behaviour, accuracy over the roast range | UNVERIFIED — **conditional requirement** |
| 5 | Solid-state relays, at least 2 units (heater + fan; add a second heater channel if used) | heater and fan switching | **zero-cross or random-fire?** input voltage, load rating, required heatsink, leakage current; the fan SSR is switched on/off only | UNVERIFIED — determines legality of time-proportioning |
| 6 | Stainless-steel heating elements, 3 units | heating | voltage, wattage, AC/DC, resistance, inrush, whether they can be paralleled, total load | UNVERIFIED — **critical** |
| 7 | Stainless-steel coffee roaster assembly | drum/bean chamber | drum vs fluid-bed, motor type/existing drive, probe ports, seals | UNVERIFIED |
| 8 | BME688 sensor | gas/VOC response | breakout board, I²C address, whether it can be placed outside the hot zone | UNVERIFIED |
| 9 | Exhaust fan | exhaust | voltage, current, AC/DC, on/off or variable-speed control, static pressure | UNVERIFIED |
| 10 | Convection fan | airflow | same as above, plus maximum temperature at the impeller | UNVERIFIED |
| 11 | 5-inch HMI touchscreen | operator interface | **exact model and resolution**: the firmware is LVGL 8 + `esp_panel` on an ESP32-S3 (`src/esp/HMI/`); the project README names the **ESP32-S3-Touch-LCD-4.3** | UNVERIFIED — `esp_panel_board_custom_conf.h` is still `ESP_PANEL_BOARD_NAME "Waveshare:ESP32-S3-Touch-LCD-7"` (800×480, ST7262) from the reference project; if the fitted panel is the 4.3" model, the display will not initialise (docs/14 B8, defect G6) |
| 12 | Wiring, connectors, fuses, enclosure | electrical assembly | conductor gauge for the measured current, temperature ratings, enclosure class, strain relief, grounding | UNVERIFIED |
| 13 | Quote line items: circuit diagrams, fuzzy programming, CAD, fabrication, integration, testing | services | which of these the supplier delivers vs what this project must produce | UNVERIFIED |

## 2. HMI decision (blocks REQ-GUI-009)

**Decision made:** the panel is a self-contained ESP32-S3 board running an
LVGL 8 touch UI with the `esp_panel` BSP — class 2 below. It is not a browser
device and it does not talk to a host; it talks to the controller over ESP-NOW
(`src/esp/HMI/`, docs/02). The earlier options table (panel PC with a browser,
serial HMI) was written when the GUI was a web dashboard and has been retired
with it.

| Class | Typical | Status |
|---|---|---|
| Panel PC / Android / Linux with browser or WebView | any 5" DSI/HDMI panel | **not used** — the browser dashboard was removed with the host |
| HDMI/SPI TFT driven by an ESP32 (LVGL-style) | 4.3"/7" 800×480 panels | **chosen**: `src/esp/HMI/` with LVGL 8 + `esp_panel` |
| Serial HMI (NEXTION family, RS-485, MODBUS) | plain touch module | not used |

What is still open is only the **physical panel**: `esp_panel_board_custom_conf.h`
in the repository is the reference project's 7" 800×480 configuration. Identify
the fitted panel (§8 step 3) and either confirm that config or replace it —
without that, the sketch may compile and show nothing (docs/14 B8, defect G6).

## 3. Electrical questions that must be answered before any mains work

Answer these with a multimeter and the element nameplates, not from memory:

1. Per heating element: rated voltage (V), rated power (W), measured cold
   resistance (Ω). → compute the operating current `I = P/V`.
2. Are the three elements wired in parallel, star, or individually switched?
   → total current and required conductor cross-section.
3. What is the actual mains supply (single-phase 230 V, split-phase, 3-phase)?
4. Does each SSR's rating exceed the load current with margin, **including
   inrush**, at the ambient temperature inside the enclosure (derate!).
5. Is the SSR zero-cross (usable with slow time-proportioning) or random-fire
   (needs phase-angle control, which this software does not implement)?
6. Are the heater SSRs independent circuits or a redundant pair? The firmware
   switches both heater channels from one command. Is the fan on its own SSR,
   and is that SSR rated for the fan's inrush?
7. What fuse/breaker rating and type protects the heater circuit, and is it on
   the line side?
8. Is there a **hardware** thermal cutoff (thermal fuse or thermostat) that
   removes heater power with the ESP32 unpowered? Where is it mounted relative
   to the heating element?
9. Is the enclosure earthed, are fixings and the drum bonded, and is there
   strain relief where the mains cable enters?
10. Are the fan and drum motor isolated from the ESP32 supply and from the
    heater circuit?

Until (1)–(5) and (8) are answered, `GEN_DUTY_CEILING_PCT` stays at its
commissioning value (30 %), which caps every heater command.

## 4. Temperature sensing arrangement (blocks REQ-TEMP-004)

The requirements explicitly warn that one probe cannot measure both environment
and bean temperature. Decide and document:

| Option | Measures | Response | Notes |
|---|---|---|---|
| Probe in the drum air / chamber | environment temperature | fast | turning point and chamber control; not bean temperature |
| Probe touching the bean mass (through the drum wall or a poppet) | bean temperature | slow, noisy when the drum turns | needed for meaningful "bean temp" claims |
| Both probes | both | — | preferred for research; costs one more MAX6675 channel/board |

If both are installed, the firmware must be told which probe feeds the RoR and
the control input. Today it consumes a single channel
(`ThermocoupleSensor`); supporting two is a documented extension, not
implemented.

## 5. Colour measurement arrangement (blocks REQ-COL-005)

The TCS3200 measures light-to-frequency behind colour filters. It does not
measure CIELAB. For repeatable results during roasting you must control or
record: illumination type and stability, distance from the bean surface,
viewing angle, optical shielding from ambient light, dust/smoke on the window,
window temperature (fogging), bean movement, and whether the measurement is of
the whole batch or a sample. The firmware only needs the resulting target ratio
(`COLOR_TARGET_RATIO` in `CONF.h`); the record of *how* it was obtained lives
in docs/07. The **engineering** of that optical fixture is not solved by this
repository.

## 6. Missing components / items to procure (provisional list)

| Item | Why it is needed | Blocking? |
|---|---|---|
| MAX6675 breakout (or equivalent) and thermocouple connector | without a cold-junction-compensated front end the temperature channel is unusable; check its logic level (many are 5 V) | yes, for any roast |
| Reference thermometer (or calibrated thermocouple meter) | to verify the thermocouple against a known standard | yes, for REQ-TEMP-005 |
| Colour reference target (a documented set of patches with measured XYZ) | to produce a real colour calibration | yes, for any L*a*b* claim |
| Independent thermal cutoff / thermostat + enclosure hardware | REQ-SAFE-001 | yes, before energising |
| Properly rated fuse/breaker, terminal blocks, cable gland, earth bonding | REQ-SAFE-008 | yes, before energising |
| SSR heatsink(s) sized for the measured load current | SSR reliability | yes |
| Opto-isolated SSR drive or driver transistors | protect the ESP32 | yes |
| 5 V supply sized for ESP32 + fans + HMI | power integrity | yes |
| Sampling arrangement for the BME688 (tube + cooling/dilution) | keep the sensor below 80 °C and dry | yes, for e-nose data |
| Microphone/envelope module + mounting | if acoustic crack detection is to be evaluated | no (operator confirmation works without it) |
| Drum motor drive / controller | if the roaster assembly does not include one | unknown |
| Spare TCS3200 and MAX6675 modules | sensor damage during commissioning | recommended |

## 7. Compatibility checks that already have an answer in software

| Check | Answer | Where |
|---|---|---|
| Does the controller need a real-time operating system? | No: one cooperative `loop()` pass with period-gated work; every interlock runs on every pass. | `ColAromatic.ino` |
| Can the control loop run without the panel? | Yes: the controller owns the session. The panel can be unplugged; the only effect is that heater authority is removed after 5 s, which is the safe direction. | `HeaterSafety.*`, docs/02 §5 |
| Is a separate computer needed for "advanced image processing"? | Not for what is implemented: colour is a ratio of three channel frequencies, computed on the ESP32. Colorimetry is deliberately absent (docs/07). | `ColorSetpoint.cpp` |
| Does the heater need PWM? | No: on/off time-proportioning with a 10 s window is used, which is what zero-cross SSRs require. | `HeaterSafety::service` |
| Does the fan need PWM? | No: it has its own SSR and is switched on/off. Any non-zero fan command runs it at full speed. | `HeaterSafety::setFan` |
| Can the ESP32 and the panel share the UART? | No need: the panel has its own ESP-NOW radio link; the UART is the service port only. | `docs/04_wiring_and_pinmap.md` |
| Is a PC needed to roast? | No. That is the point of the architecture (REQ-SYS-001). | docs/02 |

## 8. Recommended next actions (hardware, in order)

1. Photograph and transcribe the actual quotation and delivered parts into the
   table in §1 with part numbers.
2. Measure the heating elements and answer §3 questions 1–5.
3. Confirm the fitted panel model and align `esp_panel_board_custom_conf.h`
   with it (§2).
4. Decide the probe arrangement (§4) and order the second channel if needed.
5. Buy the missing safety items (§6) before assembling anything mains-powered.
6. Only then: build the control circuit **without** mains connected, verify
   logic-level switching with a lamp or LED load, and raise the duty ceiling
   only after the electrical review is documented in §3.
