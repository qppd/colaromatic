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
* The software was therefore built to be hardware-independent, with a single
  boundary (`MachineInterface`) that is already implemented for both a simulator
  and a serial-attached ESP32.

**Action required before hardware work:** attach the actual quotation,
purchase receipts and photographs of the delivered parts. Then complete the
verification column below.

## 1. Baseline inventory and verification status

| # | Item (from the requirements) | Assumed role | Must verify | Status |
|---|---|---|---|---|
| 1 | ESP32 development board | main controller | exact module, flash/RAM, free GPIOs, USB chip, logic level, whether a second UART/SPI is needed for the HMI | UNVERIFIED |
| 2 | TCS3200 colour sensor | RGB / light-to-frequency | module vs bare chip (OUT drive capability), lens, illumination, mounting, distance, shielding | UNVERIFIED |
| 3 | K-type thermocouple | temperature | probe diameter/length, sheath material, grounded/ungrounded junction, response time, max temperature, connector type | UNVERIFIED |
| 4 | MAX31856 or equivalent interface | thermocouple front end | whether one is actually in the quotation, board logic level (3.3 V safe?), cold-junction behaviour | UNVERIFIED — **conditional requirement** |
| 5 | Solid-state relays, 2 units | heater switching | **zero-cross or random-fire?** input voltage, load rating, required heatsink, leakage current | UNVERIFIED — determines legality of time-proportioning |
| 6 | Stainless-steel heating elements, 3 units | heating | voltage, wattage, AC/DC, resistance, inrush, whether they can be paralleled, total load | UNVERIFIED — **critical** |
| 7 | Stainless-steel coffee roaster assembly | drum/bean chamber | drum vs fluid-bed, motor type/existing drive, probe ports, seals | UNVERIFIED |
| 8 | BME688 sensor | gas/VOC response | breakout board, I²C address, whether it can be placed outside the hot zone | UNVERIFIED |
| 9 | Exhaust fan | exhaust | voltage, current, AC/DC, PWM or voltage control, static pressure | UNVERIFIED |
| 10 | Convection fan | airflow | same as above, plus maximum temperature at the impeller | UNVERIFIED |
| 11 | 5-inch HMI touchscreen | operator interface | **model, protocol, OS, whether it runs a browser** | UNVERIFIED — blocks GUI deployment decision |
| 12 | Wiring, connectors, fuses, enclosure | electrical assembly | conductor gauge for the measured current, temperature ratings, enclosure class, strain relief, grounding | UNVERIFIED |
| 13 | Quote line items: circuit diagrams, fuzzy programming, CAD, fabrication, integration, testing | services | which of these the supplier delivers vs what this project must produce | UNVERIFIED |

## 2. HMI decision (blocks REQ-GUI-009)

Three plausible classes of "5-inch HMI" imply completely different GUIs:

| Class | Typical | Implication |
|---|---|---|
| Panel PC / Android / Linux with browser or WebView | any 5" DSI/HDMI panel | point the browser at the host dashboard URL — **no new code** |
| HDMI/SPI TFT driven by the ESP32 (LVGL-style) | bare 800×480 panel | the ESP32 would have to render the GUI; not implemented here |
| Serial HMI (NEXTION family, RS-485, MODBUS) | plain touch module | needs a snapshot→panel adapter; not implemented here |

The delivered GUI is a **browser application**. It is verified working headless
(static assets + WebSocket + REST). Deployment on the actual panel is
**PENDING-HW** and depends on the model. Recommended action: identify the
panel, then either (a) run a browser/WebView on it, or (b) keep the panel as a
secondary display with a thin adapter, or (c) replace it — none of which should
be done without approval, since it changes the agreed scope.

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
6. Are the two SSRs independent circuits or a redundant pair? The firmware
   currently switches both from one command.
7. What fuse/breaker rating and type protects the heater circuit, and is it on
   the line side?
8. Is there a **hardware** thermal cutoff (thermal fuse or thermostat) that
   removes heater power with the ESP32 unpowered? Where is it mounted relative
   to the heating element?
9. Is the enclosure earthed, are fixings and the drum bonded, and is there
   strain relief where the mains cable enters?
10. Are the fan and drum motor isolated from the ESP32 supply and from the
    heater circuit?

Until (1)–(5) and (8) are answered, `heater.hardware_verified` stays `false`,
which caps every heater command at `commissioning_max_pct` (30 %).

## 4. Temperature sensing arrangement (blocks REQ-TEMP-004)

The requirements explicitly warn that one probe cannot measure both environment
and bean temperature. Decide and document:

| Option | Measures | Response | Notes |
|---|---|---|---|
| Probe in the drum air / chamber | environment temperature | fast | turning point and chamber control; not bean temperature |
| Probe touching the bean mass (through the drum wall or a poppet) | bean temperature | slow, noisy when the drum turns | needed for meaningful "bean temp" claims |
| Both probes | both | — | preferred for research; costs one more MAX31856 channel/board |

If both are installed, the host must be told which probe feeds the RoR control
input. Today the engine consumes a single temperature channel
(`MachineSample.temperature_c`); supporting two is a documented extension, not
implemented.

## 5. Colour measurement arrangement (blocks REQ-COL-005)

The TCS3200 measures light-to-frequency behind colour filters. It does not
measure CIELAB. For repeatable results during roasting you must control or
record: illumination type and stability, distance from the bean surface,
viewing angle, optical shielding from ambient light, dust/smoke on the window,
window temperature (fogging), bean movement, and whether the measurement is of
the whole batch or a sample. The software only requires that the calibration
record documents the conditions (`reference_id`, `measured_at`, `provenance`).
The **engineering** of that optical fixture is not solved by this repository.

## 6. Missing components / items to procure (provisional list)

| Item | Why it is needed | Blocking? |
|---|---|---|
| MAX31856 breakout (or equivalent) and thermocouple connector | without a cold-junction-compensated front end the temperature channel is unusable | yes, for any roast |
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
| Spare TCS3200 and MAX31856 modules | sensor damage during commissioning | recommended |

## 7. Compatibility checks that already have an answer in software

| Check | Answer | Where |
|---|---|---|
| Does the host need a real-time operating system? | No: the engine schedules with an injected clock and runs on CPython. | `app/engine.py` |
| Can the control loop run without the GUI? | Yes: headless engine, API optional. | `src/tools/run_simulation.py` |
| Is a separate computer needed for "advanced image processing"? | Not for the current pipeline: colour is a 4-channel transform plus a 3×4 least-squares matrix fit. A colour reference fitting step runs in milliseconds. | `processing/color.py` |
| Does the heater need PWM? | No: on/off time-proportioning with a 10 s window is used, which is what zero-cross SSRs require. | `control/heater.py` |
| Can the ESP32 and the HMI share the UART? | Unresolved — verify the HMI's interface before assigning pins. | `docs/04_wiring_and_pinmap.md` |

## 8. Recommended next actions (hardware, in order)

1. Photograph and transcribe the actual quotation and delivered parts into the
   table in §1 with part numbers.
2. Measure the heating elements and answer §3 questions 1–5.
3. Confirm the HMI class (§2) and decide the GUI deployment path.
4. Decide the probe arrangement (§4) and order the second channel if needed.
5. Buy the missing safety items (§6) before assembling anything mains-powered.
6. Only then: build the control circuit **without** mains connected, verify
   logic-level switching with a lamp or LED load, and enable
   `heater.hardware_verified` only after the electrical review.
