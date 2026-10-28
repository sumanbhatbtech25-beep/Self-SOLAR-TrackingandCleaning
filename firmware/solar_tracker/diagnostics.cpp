#include "diagnostics.h"
#include "config.h"
#include <Preferences.h>
#include <stdarg.h>

namespace {
  Preferences prefs;
  float    baselineK      = 0;   // clean-panel watts per LDR count, temperature-corrected to 25 °C
  uint32_t mismatchSince  = 0;

  float tempFactor(float tC) {
    if (isnan(tC)) return 1.0f;
    return 1.0f + TEMP_COEFF_PER_C * (tC - 25.0f);
  }

  void add(DiagReport &r, const char *code, Severity sev, const char *fmt, ...) {
    if (r.count >= DiagReport::MAX) return;
    Finding &f = r.items[r.count++];
    f.code = code;
    f.sev  = sev;
    va_list args;
    va_start(args, fmt);
    vsnprintf(f.message, sizeof(f.message), fmt, args);
    va_end(args);
    if (sev > r.worst) r.worst = sev;
  }
}

namespace Diagnostics {

void begin() {
  prefs.begin("solar", false);
  baselineK = prefs.getFloat("k", 0.0f);
}

float baseline() { return baselineK; }

bool calibrate(const SensorData &s, const TrackerStatus &t, const char **reason) {
  int light = (s.ldrEast + s.ldrWest) / 2;
  if (!s.inaOk)                         { *reason = "INA219 not found"; return false; }
  if (light < DIAG_BRIGHT_LDR)          { *reason = "Not enough light. Calibrate in direct sun"; return false; }
  if (max(s.ldrEast, s.ldrWest) >= DIAG_LDR_SATURATED) { *reason = "LDRs saturated. Use a smaller divider resistor"; return false; }
  if (t.state == TrackerState::Tracking){ *reason = "Panel still moving. Try again in a few seconds"; return false; }
  if (s.current < DIAG_MIN_CURRENT_A)   { *reason = "No panel current"; return false; }

  baselineK = (s.power / tempFactor(s.panelTempC)) / float(light);
  prefs.putFloat("k", baselineK);
  *reason = "Baseline saved";
  return true;
}

void evaluate(const SensorData &s, const TrackerStatus &t, DiagReport &r) {
  r = DiagReport();
  r.calibrated = baselineK > 0;
  int light = (s.ldrEast + s.ldrWest) / 2;
  bool bright = light >= DIAG_BRIGHT_LDR;

  // Hardware presence
  if (!s.inaOk)
    add(r, "INA219_MISSING", Severity::Crit, "INA219 not found on I2C. Check SDA/SCL (GPIO 21/22) and 3V3.");
  if (!s.tempOk)
    add(r, "TEMP_SENSOR", Severity::Warn, "DS18B20 not responding. Check GPIO 4 and the 4.7 kOhm pull-up.");

  // Night
  if (light < DARK_THRESHOLD) {
    r.night = true;
    add(r, "NIGHT", Severity::Info, "Night: parked facing sunrise.");
    return;
  }

  // Electrical
  if (s.inaOk && s.busVoltage > DIAG_OPEN_V && s.current < DIAG_MIN_CURRENT_A)
    add(r, "OPEN_CIRCUIT", Severity::Crit, "%.1f V but no current. Load side is open: check VIN-, load resistor and wiring.", s.busVoltage);
  else if (s.inaOk && bright && s.busVoltage < DIAG_NO_VOLTAGE_V)
    add(r, "NO_PANEL_VOLTAGE", Severity::Crit, "Bright light but no panel voltage. Check the fuse and panel connection.");

  if (s.tempOk && s.panelTempC > DIAG_OVERHEAT_C)
    add(r, "OVERHEAT", Severity::Crit, "Panel at %.0f C, above %.0f C. Check airflow behind the panel.", s.panelTempC, DIAG_OVERHEAT_C);

  // Tracking hardware
  if (t.stallSuspected)
    add(r, "TILT_STALL", Severity::Crit, "Tilt servo not responding: LDR imbalance is not shrinking while moving.");

  int hi = max(s.ldrEast, s.ldrWest), lo = min(s.ldrEast, s.ldrWest);
  if (bright && lo < DIAG_LDR_MISMATCH * hi) {
    if (mismatchSince == 0) mismatchSince = millis();
    if (millis() - mismatchSince >= DIAG_LDR_MISMATCH_MS)
      add(r, "LDR_MISMATCH", Severity::Warn, "%s LDR reads far lower than the other. It may be covered or disconnected.",
          s.ldrEast < s.ldrWest ? "East" : "West");
  } else {
    mismatchSince = 0;
  }

  if (t.state == TrackerState::AtLimit)
    add(r, "AT_LIMIT", Severity::Info, "At the %d degree tilt limit. Normal early and late in the day.", TILT_LIMIT_DEG);

  if (hi >= DIAG_LDR_SATURATED)
    add(r, "LDR_SATURATED", Severity::Info, "LDRs saturated in full sun. Soiling check paused; consider a smaller divider resistor.");

  // Soiling: compare against the clean-panel baseline
  if (r.calibrated && bright && hi < DIAG_LDR_SATURATED && s.inaOk && s.current >= DIAG_MIN_CURRENT_A) {
    r.expectedPower = baselineK * light * tempFactor(s.panelTempC);
    r.soilingRatio  = r.expectedPower > 0 ? s.power / r.expectedPower : NAN;
    if (!isnan(r.soilingRatio) && r.soilingRatio < DIAG_SOILING_RATIO) {
      r.soiling = true;
      add(r, "SOILING", Severity::Warn, "Output %.0f%% below expected for this light. Run a cleaning cycle.",
          (1.0f - r.soilingRatio) * 100.0f);
    }
  } else if (!r.calibrated && bright) {
    add(r, "NOT_CALIBRATED", Severity::Info, "No clean-panel baseline yet. Clean the panel, then press Calibrate in full sun.");
  }

  if (r.worst <= Severity::Info)
    add(r, "OK", Severity::Ok, "No faults detected.");
}

const char *severityName(Severity s) {
  switch (s) {
    case Severity::Ok:   return "ok";
    case Severity::Info: return "info";
    case Severity::Warn: return "warn";
    case Severity::Crit: return "crit";
  }
  return "ok";
}

}  // namespace Diagnostics
