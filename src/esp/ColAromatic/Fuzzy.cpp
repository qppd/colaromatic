#include "Fuzzy.h"

#include <math.h>

FuzzyController::FuzzyController() {
  /* Discrete output universe, identical to numpy.linspace(min, max, res). */
  const float span = GEN_FUZZY_OUT_MAX - GEN_FUZZY_OUT_MIN;
  const int last = GEN_FUZZY_OUT_RES - 1;
  for (int i = 0; i < GEN_FUZZY_OUT_RES; i++) {
    _universe[i] = GEN_FUZZY_OUT_MIN + span * ((float)i / (float)last);
  }
}

/* Piecewise-linear membership with the same shoulder behaviour as the host
   implementation: a term whose outer point is 1 stays 1 beyond the universe
   edge, a triangle whose outer point is 0 stays 0. */
float FuzzyController::membership(const GenFuzzyTerm &term, float x) {
  const float *p = term.pts;
  const int n = term.n;
  if (x <= p[0]) return p[1];
  const int li = 2 * (n - 1);
  if (x >= p[li]) return p[li + 1];
  for (int i = 0; i < n - 1; i++) {
    const float x0 = p[2 * i], y0 = p[2 * i + 1];
    const float x1 = p[2 * i + 2], y1 = p[2 * i + 3];
    if (x0 <= x && x <= x1) {
      if (x1 == x0) return y0 > y1 ? y0 : y1;
      return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
    }
  }
  return 0.0f;
}

FuzzyController::Result FuzzyController::evaluate(const float *values) {
  Result result;

  /* A required input that is missing aborts the evaluation — output 0, never
     an extrapolation (mirror: FuzzyResult(None, "missing_input")). */
  for (int i = 0; i < GEN_FUZZY_INPUT_COUNT; i++) {
    if (!GEN_FUZZY_INPUTS[i].enabled) continue;
    const float v = values[i];
    if (isnan(v) || isinf(v)) {
      result.status = STATUS_MISSING_INPUT;
      result.output = 0.0f;
      return result;
    }
  }

  float aggregate[GEN_FUZZY_OUT_RES];
  for (int k = 0; k < GEN_FUZZY_OUT_RES; k++) aggregate[k] = 0.0f;

  uint8_t fired = 0;
  for (int r = 0; r < GEN_FUZZY_RULE_COUNT; r++) {
    const GenFuzzyRule &rule = GEN_FUZZY_RULES[r];
    float strength = 1.0f;
    bool skip = false;
    for (int a = 0; a < rule.antCount; a++) {
      const GenFuzzyAnt &ant = rule.ants[a];
      const GenFuzzyInput &in = GEN_FUZZY_INPUTS[ant.input];
      if (!in.enabled) { skip = true; break; }  // rule references a disabled input
      const float v = clampTo(values[ant.input], in.umin, in.umax);
      float mu = membership(in.terms[ant.term], v);
      if (ant.negate) mu = 1.0f - mu;
      if (mu < strength) strength = mu;
    }
    if (skip) continue;
    strength *= rule.weight;
    if (!(strength > 1e-9f)) continue;

    fired++;
    const GenFuzzyTerm &outTerm = GEN_FUZZY_OUTPUT_TERMS[rule.consequent];
    for (int k = 0; k < GEN_FUZZY_OUT_RES; k++) {
      const float mu = membership(outTerm, _universe[k]);
      const float impl = mu < strength ? mu : strength;
      if (impl > aggregate[k]) aggregate[k] = impl;
    }
  }

  float total = 0.0f, weighted = 0.0f;
  for (int k = 0; k < GEN_FUZZY_OUT_RES; k++) {
    total += aggregate[k];
    weighted += _universe[k] * aggregate[k];
  }

  if (!(total > 1e-9f)) {
    result.status = STATUS_NO_RULES_FIRED;
    result.output = GEN_FUZZY_FALLBACK_PCT;
    result.fired = fired;
    return result;
  }

  float centroid = weighted / total;
  if (centroid < GEN_FUZZY_OUT_MIN) centroid = GEN_FUZZY_OUT_MIN;
  if (centroid > GEN_FUZZY_OUT_MAX) centroid = GEN_FUZZY_OUT_MAX;
  result.status = STATUS_OK;
  result.output = centroid;
  result.fired = fired;
  return result;
}
