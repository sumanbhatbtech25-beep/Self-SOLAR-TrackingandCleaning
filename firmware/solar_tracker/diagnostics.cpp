#include "diagnostics.h"
#include "config.h"
#include <Preferences.h>
#include <stdarg.h>

// Soiling is judged on panel CURRENT, not power. A PWM charge controller pins the
// panel near battery voltage, so power swings with battery state while current
// still tracks sunlight.

namespace {
  Preferences prefs;
  float    baselineK     = 0;   // clean-panel amps per LDR count
  uint32_t mismatchSince = 0;

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
  baselineK = prefs.getFloat("kI", 0.0f);
}

float baseline() { return baselineK; }

bool calibrate(const SensorData &s, const TrackerStatus &t, const BatteryData &b, const char **reason) {
  int light = (s.ldrEast + s.ldrWest) / 2;
  if (!s.inaOk)                         { *reason = "INA219 not found"; return false; }
  if (light < DIAG_BRIGHT_LDR)          { *reason = "Not enough light. Calibrate in direct sun"; return false; }
  if (max(s.ldrEast, s.ldrWest) >= DIAG_LDR_SATURATED) { *reason = "LDRs saturated. Use smaller divider resistors"; return false; }
  if (t.state == TrackerState::Tracking){ *reason = "Panel still moving. Try again in a few seconds"; return false; }
  if (b.full)                           { *reason = "Battery full, so charging is throttled. Calibrate when it is below 13.6 V"; return false; }
  if (s.current < DIAG_MIN_CURRENT_A)   { *reason = "No panel current"; return false; }

  baselineK = s.current / float(light);
  prefs.putFloat("kI", baselineK);
  *reason = "Baseline saved";
  return true;
}

void evaluate(const SensorData &s, const TrackerStatus &t, const BatteryData &b, DiagReport &r) {
  r = DiagReport();
  r.calibrated = baselineK > 0;
  int light = (s.ldrEast + s.ldrWest) / 2;
  bool bright = light >= DIAG_BRIGHT_LDR;

  // Hardware presence
  if (!s.inaOk)
    add(r, "INA219_MISSING", Severity::Crit, "INA219 not found on I2C. Check SDA/SCL (GPIO 21/22) and 3V3.");
  if (!s.tempOk)
    add(r, "TEMP_SENSOR", Severity::Warn, "DS18B20 not responding. Check GPIO 4 and the 4.7 kOhm pull-up.");

  // Battery
  if (BATTERY_ENABLED) {
    if (!b.present)
      add(r, "BATTERY_SENSOR", Severity::Warn, "Battery reads under %.0f V. Check the divider on GPIO 32, or set BATTERY_ENABLED false for bench use.", BATTERY_MISSING_V);
    else if (b.voltage > BATTERY_HIGH_V)
      add(r, "BATTERY_HIGH", Severity::Crit, "Battery at %.1f V. Overcharging: check the charge controller.", b.voltage);
    else if (b.critical)
      add(r, "BATTERY_CRITICAL", Severity::Crit, "Battery at %.1f V. Entering low-battery sleep to protect it.", b.voltage);
    else if (b.low)
      add(r, "BATTERY_LOW", Severity::Warn, "Battery at %.1f V. Charge is low.", b.voltage);
    else if (b.full)
      add(r, "BATTERY_FULL", Severity::Info, "Battery full (%.1f V). Controller is limiting charge, so output checks are paused.", b.voltage);
  }

  // Night
  if (light < DARK_THRESHOLD) {
    r.night = true;
    add(r, "NIGHT", Severity::Info, "Night: parked facing sunrise.");
    if (r.worst <= Severity::Info) add(r, "OK", Severity::Ok, "No faults detected.");
    return;
  }

  // Charging path. When the battery is full the controller cuts the current on
  // purpose, so these checks only run while the battery can still take charge.
  bool electricalFault = false;
  if (s.inaOk && !b.full) {
    if (s.busVoltage > DIAG_OPEN_V && s.current < DIAG_MIN_CURRENT_A) {
      add(r, "OPEN_CIRCUIT", Severity::Crit, "%.1f V but no current. Check INA219 VIN- to controller PV+ (wire 18).", s.busVoltage);
      electricalFault = true;
    } else if (bright && s.busVoltage < DIAG_NO_VOLTAGE_V) {
      add(r, "NO_PANEL_VOLTAGE", Severity::Crit, "Bright light but no panel voltage. Check fuse F1 and the panel wires.");
      electricalFault = true;
    } else if (bright && s.current < DIAG_MIN_CURRENT_A) {
      add(r, "NO_CHARGE", Severity::Warn, "Bright light but no charging current. Check the controller PV terminals (wires 18-19).");
      electricalFault = true;
    }
  }

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
    add(r, "LDR_SATURATED", Severity::Info, "LDRs saturated in full sun. Soiling check paused; consider smaller divider resistors.");

  // Soiling: compare panel current against the clean-panel baseline
  if (r.calibrated && bright && hi < DIAG_LDR_SATURATED && s.inaOk && !b.full && !electricalFault) {
    r.expectedCurrent = baselineK * light;
    r.expectedPower   = r.expectedCurrent * s.busVoltage;
    r.soilingRatio    = r.expectedCurrent > 0 ? s.current / r.expectedCurrent : NAN;
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
