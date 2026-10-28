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
 *   - GPIO 0, 2, 4, 5, 12, 15 are strapping pins: an external driver that
 *     holds them during boot can stop the module from starting. GPIO 5 is used
 *     for the MAX6675 CS, whose idle-high state matches the GPIO 5 boot
 *     requirement; GPIO 4 is deliberately left unused.
 *   - GPIO 23 is the default VSPI MOSI but is free here: the MAX6675 is
 *     read-only. It is a normal output, not a strapping pin or an ADC2 pin.
 *   - GPIO 16/17 are free on WROOM modules; on WROVER modules they are used
 *     by PSRAM — re-check before wiring if that module is chosen.
 */

/* --- MAX6675 K-type thermocouple (SPI, read-only) ---------------------- */
#define PIN_TC_SCK          18
#define PIN_TC_SO           19   // module SO (data out) -> ESP32 MISO
#define PIN_TC_CS            5   // active low; also idles the conversion

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
#define PIN_SSR_HEATER      26   // heater SSR, via opto-isolated driver
#define PIN_SSR_HEATER_2    13   // optional second heater SSR, same command
#define PIN_SSR_FAN         23   // fan SSR, via opto-isolated driver, on/off
                                 // only. GPIO 23 is free because the MAX6675
                                 // has no MOSI; it is not a strapping pin and
                                 // not an ADC2/Wi-Fi pin. GPIO 4 was rejected:
                                 // it is a strapping pin.
#define PIN_STATUS_LED       2   // boot strapping pin: verify it does not
                                 // interfere with the boot mode

/* --- Inputs ------------------------------------------------------------ */
#define PIN_ESTOP_IN        16   // E-stop dry contact to GND, active low,
                                 // internal pull-up enabled in software.
                                 // GPIO 35 was rejected: no internal pull-up.
#define PIN_AUDIO_IN        36   // ADC1 channel 0, envelope microphone
                                 // (input only; ADC2 is unusable with Wi-Fi)
