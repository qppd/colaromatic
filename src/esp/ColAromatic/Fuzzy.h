#pragma once

#include <Arduino.h>

#include "GenFuzzy.h"

/*
 * FuzzyTable — one complete Mamdani rule base: the input universes, the rules,
 * the output terms and the output universe they are aggregated over.
 *
 * GenFuzzy.h supplies the generated temperature controller table
 * (generatedFuzzyTable(), baked from src/config/controllers/*.json).
 * ColorSetpoint.h supplies a small hand-written table that maps a colour error
 * to a temperature setpoint. Both run through the SAME inference code below,
 * so the algorithm cannot drift between them.
 *
 * The output universe always uses GEN_FUZZY_OUT_RES points; only the range
 * differs between tables. A setpoint table therefore has a resolution of
 * (outMax - outMin) / (GEN_FUZZY_OUT_RES - 1) °C.
 */
struct FuzzyTable {
  const GenFuzzyInput *inputs;
  uint8_t inputCount;
  const GenFuzzyRule *rules;
  uint8_t ruleCount;
  const GenFuzzyTerm *outTerms;
  float outMin;
  float outMax;
  float fallback;
};

/* The generated temperature -> duty table (GenFuzzy.h). */
const FuzzyTable &generatedFuzzyTable();

/*
 * FuzzyController — onboard Mamdani inference (REQ-FUZ-001..004).
 *
 * A direct port of src/colaromatic/control/fuzzy.py so the machine can run
 * automatic control with no PC attached:
 *
 *   - piecewise-linear membership with shoulder clamping at the universe edge
 *   - min implication, max aggregation, centroid defuzzification on a
 *     discrete output universe of GEN_FUZZY_OUT_RES points
 *   - rules whose antecedent references a disabled input are skipped
 *   - missing (NAN) required input -> status missing_input, output 0
 *   - no rule fired -> the table's fallback output — never a guess
 *
 * The controller never provides safety protection: interlocks and latches in
 * HeaterSafety remove heater authority independently of any inference result.
 */
class FuzzyController {
 public:
  enum Status : uint8_t {
    STATUS_OK = 0,
    STATUS_MISSING_INPUT = 1,
    STATUS_NO_RULES_FIRED = 2,
  };

  struct Result {
    float output = 0.0f;   // in the table's output universe; 0 when not computable
    uint8_t status = STATUS_MISSING_INPUT;
    uint8_t fired = 0;     // how many rules fired (diagnostics)
  };

  explicit FuzzyController(const FuzzyTable &table = generatedFuzzyTable());

  /* values: one entry per table.inputCount, NAN marks a missing input. */
  Result evaluate(const float *values);

  static const char *statusText(uint8_t status) {
    switch (status) {
      case STATUS_OK: return "ok";
      case STATUS_MISSING_INPUT: return "missing_input";
      default: return "no_rules_fired";
    }
  }

 private:
  static float membership(const GenFuzzyTerm &term, float x);
  static float clampTo(float value, float lo, float hi) {
    return value < lo ? lo : (value > hi ? hi : value);
  }

  const FuzzyTable *_table;
  float _universe[GEN_FUZZY_OUT_RES];
};
