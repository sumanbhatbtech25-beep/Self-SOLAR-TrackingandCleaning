// diagnostics.h — dust (soiling) check, current-sensor cross-check,
// relay self-test and the rule-based fault list.
#pragma once
#include <Arduino.h>

namespace Diagnostics {
  void begin();
  void afterPowerRead();           // every second: soiling ratios + sensor arbitration
  void soilingCheck();             // every SOILING_INTERVAL_MS: confirm dust before alerting
  bool soilingAlert();
  void evaluateFaults();           // every second: rebuilds app.faults
  bool calibrate(String &msg);     // clean panel in steady light: store the baseline
  bool startSelfTest(String &msg);
  void updateSelfTest();           // call every loop
  bool selfTestActive();
  void zeroAcsAtBoot();            // relay 1 opens briefly so the ACS712 zero is measured
  const char *describe(const String &code);
}
