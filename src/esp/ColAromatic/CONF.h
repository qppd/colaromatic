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
#define SENSOR_MIN_TEMP_C       -20.0f // outside this the reading is invalid
#define SENSOR_MAX_TEMP_C       400.0f // matches host temperature.max_valid_c
#define LOCAL_MAX_TEMP_C        250.0f // on-board backstop, above the host's
                                       // 240 C limit so the host trips first
#define LOCAL_OVERTEMP_HYST_C   20.0f  // must fall this far below before the
                                       // backstop re-arms

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

/* --- Protocol ---------------------------------------------------------- */
#define FIRMWARE_BANNER         "# ColAromatic controller ready (PROVISIONAL FIRMWARE)"
