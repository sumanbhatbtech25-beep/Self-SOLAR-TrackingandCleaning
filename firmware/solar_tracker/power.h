// power.h — battery monitoring, low-battery protection and deep sleep.
//
// The battery is measured through a 100 kΩ / 27 kΩ divider on GPIO 32.
// Below BATTERY_CUTOFF_V the board deep-sleeps and only wakes properly once
// the battery has recovered above BATTERY_RESUME_V. At night it can also
// deep-sleep in 15-minute naps to save energy.
#pragma once
#include <Arduino.h>

struct BatteryData {
  float voltage  = NAN;   // V
  int   percent  = -1;    // rough estimate from voltage, -1 if unknown
  bool  present  = false; // divider connected and reading a plausible voltage
  bool  full     = false; // controller is limiting charge
  bool  low      = false;
  bool  critical = false;
};

enum class SleepReason : uint8_t { None = 0, LowBattery = 1, Night = 2 };

namespace Power {
  // Call first in setup(). May put the board straight back to sleep
  // (low battery still not recovered, or still dark after a night nap).
  void beginEarly();
  void update(BatteryData &out);
  float readBatteryVoltage();
  [[noreturn]] void deepSleep(uint32_t minutes, SleepReason why, int tiltToRestore);
  SleepReason lastSleepReason();   // why we were asleep before this boot
  bool hasSavedTilt();
  int  savedTilt();
}
