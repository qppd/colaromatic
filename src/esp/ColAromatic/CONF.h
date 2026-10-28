#pragma once

#include <stdint.h>

/* Values that come from the host-side configuration (src/config/system.json)
   are NOT duplicated here: they are baked into GenConfig.h by
   src/tools/gen_firmware_tables.py so the machine and the offline analysis
   tools can never drift apart. Regenerate after editing system.json. */
#include "GenConfig.h"

/*
 * ColAromatic — firmware configuration
 * --------------------------------------
 * SINGLE SOURCE OF TRUTH for every timing, threshold and protocol constant.
 * Change values here only; the modules read them through these macros.
 *
 * Every threshold marked REQUIRES_CALIBRATION is an engineering placeholder.
 * They must be re-measured on the real machine (docs/09_test_plan.md) before
 * any claim of verified behaviour.
 *
 * Safety-relevant relationships that must stay true if you edit these:
 *   - HOST_TIMEOUT_MS  >  host keepalive period (system.json
 *                         timing.keepalive_period_s = 1.0 s). If it is not,
 *                         the watchdog drops heater authority mid-roast.
 *   - LOCAL_MAX_TEMP_C >  host safety.max_temperature_c (240 C) so the host
 *                         trips first and the controller is the backstop.
 *   - Report cadence (REPORT_PERIOD_MS) is unrelated to the interlock cadence
 *     (TEMP_INTERLOCK_PERIOD_MS): the interlock must not wait for a report.
 */

/* --- Serial link ------------------------------------------------------- */
#define SERIAL_BAUD             115200
/* readStringUntil() would otherwise block for the 1000 ms Arduino default,
   which stalls every interlock evaluation while a partial line is in flight. */
#define SERIAL_TIMEOUT_MS       50

/* --- Cadences (firmware-only; process timings live in GenConfig.h) ------ */
#define REPORT_PERIOD_MS        1000   // T/C/E/H frame cadence
#define TEMP_INTERLOCK_PERIOD_MS 250    // thermocouple sampling for the local
                                        // interlock (faster than reporting)
#define COLOR_WINDOW_MS         50     // integration window per colour channel
#define AUDIO_SAMPLE_WINDOW_MS  10     // peak-to-peak envelope window
#define FAULT_REPORT_THROTTLE_MS 1000  // minimum gap between repeats

/* --- Host watchdog (REQ-FW-002) ---------------------------------------- */
#define HOST_TIMEOUT_MS         3000   // no recognized host frame -> heater off

/* --- Emergency stop ---------------------------------------------------- */
#define ESTOP_DEBOUNCE_MS       20     // contact must be stable this long

/* --- Temperature (REQUIRES_CALIBRATION) -------------------------------- */
/* MAX6675 conversion time, datasheet maximum. Reading faster than this only
   returns the previous conversion, so the interlock cadence must not be
   shorter. */
#define MAX6675_CONVERSION_MS   220
#if TEMP_INTERLOCK_PERIOD_MS < MAX6675_CONVERSION_MS
#error "TEMP_INTERLOCK_PERIOD_MS must be >= MAX6675_CONVERSION_MS"
#endif

#define SENSOR_MIN_TEMP_C       0.0f   // MAX6675 saturates at 0 C; it cannot
                                       // report negative temperatures
#define SENSOR_MAX_TEMP_C       400.0f // matches host temperature.max_valid_c
#define LOCAL_MAX_TEMP_C        250.0f // on-board backstop, above the host's
                                       // 240 C limit so the host trips first
#define LOCAL_OVERTEMP_HYST_C   20.0f  // must fall this far below before the
                                       // backstop re-arms

/* --- Colour -> setpoint outer loop (REQUIRES_CALIBRATION) --------------- */
/* The colour input is an uncalibrated browning proxy, not a colour
   measurement: COLOR_TARGET_RATIO must be measured on the real machine
   against a colour reference target before the automatic target means
   anything (docs/13_known_limitations.md). */
#define COLOR_TARGET_RATIO      0.45f  // red/(red+green+blue) at the target roast
#define COLOR_MIN_SUM_HZ        30.0f  // below this the sensor sees no light
#define COLOR_SETPOINT_MIN_C    150.0f // fuzzy output universe, °C
#define COLOR_SETPOINT_MAX_C    230.0f // must stay below LOCAL_MAX_TEMP_C

/* --- PID temperature control (REQUIRES_CALIBRATION) -------------------- */
/* Parallel form, gains per second:
     duty% = kp*e + ki*∫e dt - kd*dT/dt,  e = setpoint - filtered temperature
   Placeholders sized for the 2 s control period and the 30 % commissioning
   ceiling. ki is small on purpose: the drum has a long lag, and the integral
   is what overshoots if it is tuned on a cold machine. kd is 0 because the
   MAX6675 resolves 0.25 °C and a derivative on that quantisation amplifies
   noise more than it helps; raise it only with a measured noise figure. */
#define PID_KP                  1.5f  // % per °C
#define PID_KI                  0.03f // % per (°C·s)
#define PID_KD                  0.0f  // % per (°C/s)
#define PID_INTEGRAL_MIN_PCT    (-20.0f) // hard bound on the stored I term
#define PID_INTEGRAL_MAX_PCT    100.0f

/* --- Acoustic crack detection (disabled until calibrated) -------------- */
#define AUDIO_ENABLED_DEFAULT   0
#define AUDIO_THRESHOLD_DEFAULT 0      // raw ADC peak-to-peak counts
#define AUDIO_COOLDOWN_MS       1500

/* --- BME688 ------------------------------------------------------------- */
#define ENOSE_WARMUP_MS         60000UL  // placeholder; see docs/13
#define ENOSE_I2C_ADDR_LOW      0x76
#define ENOSE_I2C_ADDR_HIGH     0x77

/* --- HMI link (ESP-NOW) ------------------------------------------------ */
/* Frame layout and the beacon/state/timeout contract live in LinkFrames.h;
   they are shared verbatim with the HMI sketch. */
#define HMI_PAIR_MAGIC          0x5A   // OP_PAIR argument that reopens pairing

/* The touch panel's station MAC. The controller prints its own MAC at boot and
   the panel prints its own; paste each into the other's firmware so the two
   boards address each other directly instead of relying on the pairing
   handshake. All zero keeps the auto-pair behaviour (first HMI beacon wins).
   A MAC already stored in NVS takes precedence over this constant, so
   OP_PAIR re-pairing still works after a board swap. */
#define HMI_PEER_MAC            {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

/* --- Protocol ---------------------------------------------------------- */
#define FIRMWARE_BANNER         "# ColAromatic controller ready (PROVISIONAL FIRMWARE)"
