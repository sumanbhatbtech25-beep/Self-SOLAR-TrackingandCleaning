// diagnostics.h — rule-based fault checks. Every rule is listed in docs/diagnostics.md.
#pragma once
#include <Arduino.h>
#include "sensors.h"
#include "tracker.h"
#include "power.h"

enum class Severity : uint8_t { Ok = 0, Info = 1, Warn = 2, Crit = 3 };

struct Finding {
  const char *code = "";          // stable id, e.g. "OPEN_CIRCUIT"
  Severity    sev  = Severity::Ok;
  char        message[112] = {0};
};

struct DiagReport {
  static constexpr int MAX = 10;
  Finding  items[MAX];
  int      count          = 0;
  Severity worst          = Severity::Ok;
  bool     night          = false;
  bool     soiling        = false;  // drives auto-clean
  bool     calibrated     = false;  // a clean-panel baseline exists
  float    soilingRatio   = NAN;    // actual / expected panel current, NAN when not measurable
  float    expectedCurrent = NAN;   // A
  float    expectedPower  = NAN;    // W, expected current × present voltage
};

namespace Diagnostics {
  void begin();   // loads the saved baseline from flash
  void evaluate(const SensorData &s, const TrackerStatus &t, const BatteryData &b, DiagReport &out);
  // Record "this is the current a clean panel gives for this much light".
  // Returns false (with a reason) if conditions aren't suitable.
  bool calibrate(const SensorData &s, const TrackerStatus &t, const BatteryData &b, const char **reason);
  float baseline();   // A per LDR count; 0 if not calibrated
  const char *severityName(Severity s);
}
