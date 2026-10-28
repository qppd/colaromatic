# 12 — Troubleshooting guide

Every entry states the symptom, the likely cause, the immediate action, and how
to confirm the diagnosis. Safety-relevant entries are marked **SAFETY**.

There is no server, no browser and no log file: the two sources of truth are the
**panel screens** (dashboard, diagnostics) and the **USB serial output** at
115200. Read the diagnostics screen first — link state, active faults, latched
faults and the controller event list are all there.

## 1. Start-up and link

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| Panel stays on the boot screen | LVGL/board configuration does not match the fitted panel (the repository still carries the 7″ reference config — docs/14 B8) | check `esp_panel_board_custom_conf.h` against the actual panel | dashboard appears after ~3 s |
| Panel header reads `OFFLINE`, tiles show `--` | peer MAC wrong, boards on different channels, panel booted before the controller, or radio init failed | power the controller first; verify `HMI_PEER_MAC` / `CONTROLLER_PEER_MAC`; reflash | Diagnostics: `connected`, `paired: yes`, frame age "just now" |
| `paired: no` | pairing was opened, or NVS peer was cleared | complete the pairing handshake, or re-burn the MAC | `paired: yes`, state frames arrive within 5 s of boot |
| Values arrive but never change | the controller is sending STATE frames built from a dead sensor stream | check serial `T` frames; check probe wiring | dashboard quality line stops reading `no telemetry` |
| Controller prints `# controller MAC …` but nothing else | normal: that line is emitted before any init that can fail | wait; then check `ERR,ESP_NOW_INIT_FAILED` | if present, WiFi/ESP-NOW init failed — reflash, check the board target |
| `ERR,ESP_NOW_INIT_FAILED` at boot | radio not initialised (wrong board target, or RF disabled) | reflash with the correct FQBN | no such error on the next boot |
| Serial monitor shows garbage | wrong baud | 115200, 8N1 | readable banner |

## 2. Sensors

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| Temperature `--` / `T` frame empty, or `start refused` for no valid temperature | probe disconnected, broken thermocouple, wrong CS pin, MAX6675 unpowered, or a 5 V module driving a 3.3 V ESP32 | check connector and polarity; check SCK/SO/CS against `PINS.h`; confirm the module's logic level | plausible ambient value on the dashboard; `T` frame flag `0` |
| `ERR,TC_OPEN` immediately at boot | MAX6675 unpowered, miswiring, or an unplugged module pulled to 0xFFFF | recheck wiring and supply | no fault on boot |
| `ERR,TC_OPEN` / `TC_FAULT` during a roast | broken thermocouple wire, loose connector, non-K probe, reading outside range | **the firmware drops the heater within one loop pass**; stop the roast | `T` frames carry flag `2` and an empty value |
| `ERR,TC_INVALID_HEATER_OFF` | the heater trip caused by the fault above | fix the probe; a **fresh START** is required — the machine does not resume by itself | heater stays off while the probe is invalid |
| Temperature implausible or jumping | intermittent contact, corrosion, or a reading outside the firmware's valid range | inspect the connector | serial `T` frames agree with the dashboard |
| RoR shows `--` at the start | fewer than the required samples, or span under the minimum | wait ~10 s | RoR appears once the window fills |
| RoR jumps wildly | noisy probe or loose connection | check seating; consider a longer RoR window in `CONF.h` | smoother trace on Curves |
| Colour status `invalid` | clear channel outside `[GEN_COLOR_MIN_HZ, GEN_COLOR_MAX_HZ]` — too dark, too bright, or wiring fault | clean the window, check illumination and the sensor supply | status returns to `uncalibrated` |
| Colour status `uncalibrated` | **expected** — no calibration exists (docs/07) | none | status stays `uncalibrated` until §3 of docs/07 is done |
| Colour values drift during a roast | window fouling, lamp drift, smoke scatter | clean the window | re-measure before/after and compare the ratio |
| Gas values `--` / `ERR,BME_INIT_FAIL` | I²C address (0x76/0x77), pull-ups, damaged sensor | try the other address; scan the bus | `E` frames with flag `0` |
| `ERR,BME_READ_FAIL` | transient I²C failure | check bus length and pull-ups | `E` frames return to flag `0` or `1` |
| `F_ENOSE_HOT` warning | sensor above its limit | improve the sampling arrangement — **warning only**, the heater is not interlocked on it | warning clears |

## 3. Control and heater

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| Duty stays at 0 % in AUTOMATIC | no usable colour reading (falls back to the trajectory/operator target), no RoR yet, or an interlock removed authority | check Diagnostics for active faults; check the quality line | duty rises once inputs are valid and faults are clear |
| AUTOMATIC refuses to start | the profile is `PROVISIONAL` and `autoEligible = 0` — **all 12 are** | use MANUAL; automatic needs a validated profile (docs/01 §B) | refusal popup states the reason |
| Duty is capped below what you asked | the effective ceiling (commissioning cap, profile limit) — the panel cannot raise it | read the ceiling the controller reports; it only moves after the electrical review | applied duty ≤ ceiling on the dashboard tile |
| Temperature never reaches the target | ceiling too low for the plant | verify with a fixed-duty baseline; raise the ceiling only after the electrical review | plateau temperature vs duty |
| Temperature overshoots badly | untuned PID, unvalidated fuzzy surface, or a much faster plant than assumed | switch to MANUAL for the rest of the run; re-tune per docs/06 §6 | step-response record |
| Duty oscillates rapidly | control period too short, or minimum on/off too small | lengthen the control period, or increase `GEN_HEATER_MIN_ON_MS`/`MIN_OFF_MS` | steadier relay on-time |
| Relay does not follow the command | SSR wiring, drive level, wrong SSR type, stuck contact | **SAFETY** remove mains and inspect; verify with a lamp load | commanded vs observed on a scope or lamp |
| Fan does not run during cooling | fan SSR wiring, supply, or stuck SSR | **SAFETY** stop and inspect; do not leave a hot batch hot | air movement at the exhaust; falling temperature |
| Fan speed cannot be varied | expected: the fan SSR is on/off, any non-zero command is full speed | none — this is hardware, not a setting | serial `H` frame fan field is `0` or `100` |
| Commands from the serial port are refused with `SESSION_ACTIVE` | a roast session owns the heater; the service port may not drive outputs during one | stop the session, or use the panel | refusal clears when the session ends |
| Machine will not heat at all, ever | no independent thermal cutoff installed (**SAFETY** — do not work around it), or a latched fault | read Diagnostics; install the cutoff before any energising test | faults clear; `relay` toggles on the `H` frame |

## 4. Safety faults

Codes as they appear on the panel (fault bits, `LinkFrames.h`) and on serial
(`ERR,<code>`):

| Code | Meaning | Action |
|---|---|---|
| `TC_INVALID_HEATER_OFF` / `F_TEMP_INVALID` | no usable temperature | fix the probe; the heater is already off; fresh START needed |
| `F_TEMP_STALE` | readings stopped arriving | fix the probe/link |
| `OVER_TEMPERATURE` / `F_OVER_TEMP` | process limit reached | **latched**: cool, inspect, investigate, then RESET FAULTS while cool |
| `LOCAL_OVER_TEMP` / `F_LOCAL_OVERTEMP` | firmware backstop (`CONF.h` `LOCAL_MAX_TEMP_C`) | heater off; clears on hysteresis; investigate |
| `ROR_RUNAWAY` / `F_ROR_RUNAWAY` | RoR above `GEN_MAX_ROR_CPM` for 3 consecutive sensor ticks | **latched**; verify the threshold against a real roast before relying on it |
| `ESTOP_INPUT` / `F_ESTOP` | physical E-stop held | release the switch, then RESET; refused while hot |
| operator E-stop (panel button or `CMD,ESTOP`) | software E-stop latched | same: clear the hazard, wait for cool, then reset |
| `F_HMI_LINK_LOST` | no panel frame for 5 s | check the panel power and the peer MAC; heater authority was removed |
| `HOST_TIMEOUT` / `F_HOST_TIMEOUT` | no recognized serial command for 3 s after a host was live | re-attach and re-send a command |
| `ESTOP_STILL_ACTIVE` | reset attempted while the E-stop input is still held | release the switch |
| `F_ENOSE_HOT` | e-nose sensor over temperature | **warning only** — no heater interlock |

A trip **clears the duty request as well as the output**: after any of these,
recovery needs a fresh START from a live operator. Latched faults additionally
need RESET FAULTS while the batch is cool; the refusal reason is shown.

## 5. Recording and data

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| Nothing was saved after a roast | **there is no recorder** — no SD writer, no session file (docs/01 §J) | photograph the screens; copy values from Curves and Diagnostics by hand | this is the documented state, not a fault |
| Event list on Diagnostics is short | the event log is a small volatile ring buffer in panel RAM | record events as they happen | older events roll off |
| Curves restart when you re-open the screen | history is kept only while the screen is open | leave the screen open during a trial | series accumulate |
| Curves are flat | no telemetry since the screen was opened | check the link age on Diagnostics | values move again |

## 6. Firmware and service port

| Symptom | Likely cause | Immediate action | Confirmation |
|---|---|---|---|
| `ERR,UNKNOWN_COMMAND` | a sent line is not in the command table, or a version mismatch with docs/05 | compare your tool's commands with docs/05 §2 | no such errors after fixing the sender |
| Watchdog trips although a tool is attached | only **recognized** commands refresh it; noise does not | send `PING` or a valid `SET,…` | no `HOST_TIMEOUT` while your tool is sending valid commands |
| `ERR,RESET_OK` never appears | reset refused while latched and hot, or the E-stop input is held | read the refusal text | resets once cool and released |
| Firmware resets unexpectedly | power brownout, WDT, or a crash | check supply and serial banner; the MAC line reprints on every boot | banner reappears; **latches are cleared by a reset**, so re-check faults before trusting the state |
| Sketch will not compile | header drift between the two sketches (`LinkFrames.h`, `GenProfiles.h` are copied into `src/esp/HMI/`) | copy the controller's file over the panel's copy and rebuild | both compile |
| Panel compiles but the display stays dark | board config mismatch (7″ reference vs the fitted panel) | docs/14 B8; set the right `esp_panel` board config | splash appears |

## 7. When in doubt

1. Stop the roast (**STOP & COOL**), or **E-STOP** if there is any doubt about
   the heater.
2. Read the fault list on the Diagnostics screen and locate it in §4.
3. Copy the event list and take photographs — **they are the only evidence you
   will get**; nothing is persisted.
4. Do not restart into AUTOMATIC mode until the cause is understood and the
   pre-flight checks in docs/11 §3 pass again.
