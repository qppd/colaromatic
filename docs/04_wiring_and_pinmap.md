# 04 — Wiring, pin map and power design (PROVISIONAL)

> **These documents are provisional.** No cable has been cut, no circuit has
> been assembled or measured, and no mains voltage has been applied in the
> development of this repository. Do not treat anything here as a validated
> mains wiring instruction. Complete
> [03 — Hardware BOM gap analysis](03_hardware_bom_gap_analysis.md) §3 first and
> have the electrical design reviewed.

## 1. Signal-level wiring (low voltage only)

The authoritative list is `src/esp/ColAromatic/PINS.h` — this table mirrors it
and must be updated in the same change.

| ESP32 GPIO | Signal | Direction | Notes / risk |
|---|---|---|---|
| 18 | MAX6675 SCK | out | SPI clock |
| 19 | MAX6675 SO | in | module data out (MISO). **Many MAX6675 breakouts are 5 V — level-shift this line** |
| 5 | MAX6675 CS | out | chip select, active low. GPIO 5 is a strapping pin, but its boot state (HIGH) is also the CS idle state |
| 21 | BME688 SDA | bidir | I²C, needs pull-ups (usually on the breakout) |
| 22 | BME688 SCL | out | I²C clock |
| 32 | TCS3200 S0 | out | frequency scaling select |
| 33 | TCS3200 S1 | out | frequency scaling select |
| 27 | TCS3200 S2 | out | filter select |
| 14 | TCS3200 S3 | out | filter select |
| 34 | TCS3200 OUT | in | **input-only pin**, no internal pull-up; verify the module can drive it |
| 25 | TCS3200 LED | out | reserved; illumination control is external |
| 26 | Heater SSR 1 | out | through an opto-isolated input / driver, never directly |
| 13 | Heater SSR 2 | out | optional second heater channel, same command |
| 23 | Fan SSR | out | on/off only, through an opto-isolated input / driver; **no PWM**. Free because the MAX6675 has no MOSI; not a strapping pin or an ADC2 pin |
| 4 | — | — | **free; strapping pin** — do not use for an output that could hold it during boot |
| 16 | E-stop input | in | active low, **internal pull-up enabled**; dry contact to GND. GPIO 35 was rejected: input-only pins have no internal pull-up, so `INPUT_PULLUP` there would silently do nothing and leave the input floating |
| 36 | Audio envelope | in | ADC1 channel 0 only (ADC2 is unusable with Wi-Fi on) |
| 2 | Status LED | out | boot strapping pin: verify it does not fight the boot mode |

**Unresolved conflicts to check before assembly**

* GPIO 6–11 are used by the on-board flash on most modules — never use them.
* GPIO 0/2/4/5/12/15 are strapping pins; an external driver on them can prevent
  boot. GPIO 5 is used for the MAX6675 CS, whose idle-high state matches the
  GPIO 5 boot requirement; GPIO 4 is deliberately left unused.
* GPIO 34–39 are input-only and have **no** internal pull-up — never use one
  for the E-stop, which is why the E-stop sits on GPIO 16.
* If a NEXTION/RS-485/serial HMI is used, it will consume the UART pins (1/3 or
  16/17) and possibly more; reassign the pin map before wiring.
* If Wi-Fi is used for the GUI, keep analogue inputs on ADC1 and expect Wi-Fi
  power transients.

## 2. Electrical block diagram (logic level)

```
           +5 V ──┬───────────────┬───────────────┬────────────────┐
                  │               │               │                │
              ESP32 dev       fan supply      HMI supply     SSR drive
                  │               │               │            (opto)
    ┌─────────────┼───────────────┴───────────────┘                │
    │             │                                                │
 SPI│        I²C │                                TTL direction     │
 ┌──▼──────┐ ┌───▼────┐  ┌───────────┐  ┌──────────┐  ┌──────────┐ │
 │MAX6675  │ │ BME688 │  │ TCS3200   │  │ E-stop   │  │ audio    │ │
 │ + K-TC  │ │        │  │ + lamp    │  │ switch   │  │ envelope │ │
 └─────────┘ └────────┘  └───────────┘  └──────────┘  └──────────┘ │
                                                                    │
        ── SEPARATE, ISOLATED, MAINS-SIDE CIRCUIT (review required) ──▼──────
        L ──[ fuse / breaker ]──[ hardware thermal cutoff ]──┬── SSR H1 ── heater 1 ──┐
                                                             ├── SSR H1/H2 ─ heater 2 ─┤
                                                             ├── SSR H2 ── heater 3 ─┤
                                                             └── SSR F ─── fan ──────┤
                                                                                   N ─┘
        earth bonding of the drum, enclosure, element bodies, and any exposed metal
```

Everything above the dashed line is low voltage. Everything below it is the
hazardous side and depends entirely on the real element ratings measured in
§3 of document 03.

The heater and the fan have separate SSRs. The heater channels are driven by
time-proportioning; the fan SSR is switched on/off only (no PWM) and must be
rated for the fan motor's inrush current. The H1/H2 split above is one possible
arrangement: the firmware supports one heater SSR or two driven together.
Confirm the actual element and SSR count before wiring.

## 3. Power and protection checklist (all items are PENDING-HW)

| # | Requirement | Evidence to capture |
|---|---|---|
| 1 | Fuse or breaker rated for the measured total heater current, on the line side | rating, type, photo |
| 2 | Hardware thermal cutoff (thermal fuse or thermostat) in series with the heater, independent of the ESP32 | part number, trip temperature, bench test with the controller powered **off** |
| 3 | SSR load rating derated for enclosure ambient and inrush (heater **and** fan SSR) | datasheet figures + measured current |
| 4 | SSR heatsink with the correct mounting torque and interface material | photo + thermal measurement after 10 min at full duty |
| 5 | Opto-isolation or driver stage between ESP32 and each SSR input | schematic + scope capture of the gate signal |
| 6 | Correct conductor cross-section and high-temperature insulation inside the hot zone | gauge, insulation rating |
| 7 | Earth continuity from the mains plug to every exposed metal part | measured resistance |
| 8 | Enclosure, grommets and strain relief on every cable entry | photo |
| 9 | Fan supplied independently of the ESP32 rails and isolated from the heater circuit | schematic |
| 10 | Startup and shutdown sequences: heater off at start, heater off on stop/estop/fault, cooling continues | log excerpt from a real test |
| 11 | Mains isolation switch that is reachable while the roaster is running | photo |
| 12 | Emergency stop cuts heater power by a **hardware** path, not only via software | bench test with the controller disconnected |
| 13 | Pull-down on each SSR input so the heater and fan stay off while the ESP32 boots or resets | schematic + boot test with a lamp load |

## 4. What the software does and does not do about all this

| Software behaviour | Where | What it cannot do |
|---|---|---|
| Never energises the heater before a session starts | `engine.start_session` writes `SET,HEATER,0` | a welded SSR contact |
| Caps duty at the commissioning ceiling until the circuit is verified | `config.system.json` → `heater.hardware_verified`, `commissioning_max_pct` | an SSR that ignores its input |
| Removes heater authority on fault, staleness, link loss, estop | `control/safety.py`, `engine._force_heater_off` | break the mains circuit by itself |
| Keeps cooling and ventilation running after a stop | `engine._begin_cooling` | prove that the fan is actually running (no feedback input today) |
| Records every heater command change as an event | `engine._event("heater_command")` | detect a relay that did not follow the command unless the firmware feedback line is wired and `feedback_expected` is enabled |
| Firmware re-evaluates every interlock on every loop pass (probe fault, E-stop, over-temperature, host silence) | `src/esp/ColAromatic/HeaterSafety.cpp::service()` | a firmware crash that leaves the output latched — hence the hardware cutoff |
| Firmware turns the heater off after 3 s without host frames, and the host re-asserts its command every second so a live host never trips that watchdog | `CONF.h` `HOST_TIMEOUT_MS`, `engine.step()` keepalive | a USB adapter that keeps receiving while stopping delivering |

## 5. Recommended commissioning order (no beans, no mains first)

1. Flash the firmware and watch the serial monitor with **no mains connected**.
2. Verify every sensor frame: thermocouple in ice water and boiling water,
   colour sensor against a grey card, BME688 in still air.
3. Verify the E-stop input and the host watchdog by pulling the USB cable while
   the heater outputs are connected to LEDs instead of SSRs.
4. Connect the heater and fan SSRs with the loads **disconnected**; verify
   switching with a lamp load and confirm the commanded heater duty on a scope.
   Confirm the fan SSR is on for any non-zero fan command and off at zero.
5. Connect one heating element and confirm the measured current matches the
   nameplate calculation, with the thermal cutoff temporarily set low to prove
   it trips.
6. Only then run a dry roast (no beans) at the commissioning ceiling and record
   the chamber temperature response.
7. Record every step in `docs/09_test_plan.md` §C with photos and instrument
   readings.
