#pragma once

/*
 * ColAromatic — GPIO assignments
 * --------------------------------
 * SINGLE SOURCE OF TRUTH. Every pin used by the firmware is named here and
 * nowhere else. The table is mirrored in docs/04_wiring_and_pinmap.md, which
 * must be updated in the same change.
 *
 * STATUS: PROVISIONAL. Verify each line against the actual board module and
 * the wiring harness before applying power. Changing a pin means changing
 * this file only — recompile and re-run the pre-flight checks.
 *
 * ESP32 constraints that drove these choices:
 *   - GPIO 6..11 are connected to the on-board flash: never use them.
 *   - GPIO 34..39 are input only and have NO internal pull-up. `pinMode(x,
 *     INPUT_PULLUP)` on those pins silently does nothing, which would leave a
 *     floating E-stop input. The E-stop is therefore not on 34..39.
 *   - GPIO 0, 2, 12, 15 are strapping pins: an external driver that holds
 *     them during boot can stop the module from starting.
 *   - GPIO 16/17 are free on WROOM modules; on WROVER modules they are used
 *     by PSRAM — re-check before wiring if that module is chosen.
 */

/* --- MAX31856 K-type thermocouple (SPI) -------------------------------- */
#define PIN_TC_SCK          18
#define PIN_TC_MISO         19
#define PIN_TC_MOSI         23
#define PIN_TC_CS            5

/* --- TCS3200 light-to-frequency colour sensor -------------------------- */
#define PIN_TCS_S0          32   // frequency scaling select
#define PIN_TCS_S1          33   // frequency scaling select
#define PIN_TCS_S2          27   // photodiode filter select
#define PIN_TCS_S3          14   // photodiode filter select
#define PIN_TCS_OUT         34   // input only, no pull-up; module must drive it
#define PIN_TCS_LED         25   // reserved; illumination is external today

/* --- BME688 gas sensor (I2C) ------------------------------------------- */
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

/* --- Outputs ----------------------------------------------------------- */
#define PIN_SSR_1           26   // heater SSR channel 1 (via opto driver)
#define PIN_SSR_2           13   // heater SSR channel 2 (via opto driver)
#define PIN_FAN              4   // exhaust / convection fan, LEDC PWM
#define PIN_STATUS_LED       2   // boot strapping pin: verify it does not
                                 // interfere with the boot mode

/* --- Inputs ------------------------------------------------------------ */
#define PIN_ESTOP_IN        16   // E-stop dry contact to GND, active low,
                                 // internal pull-up enabled in software.
                                 // GPIO 35 was rejected: no internal pull-up.
#define PIN_AUDIO_IN        36   // ADC1 channel 0, envelope microphone
                                 // (input only; ADC2 is unusable with Wi-Fi)
