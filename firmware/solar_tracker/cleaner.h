// cleaner.h — non-blocking wiper cycle on the second servo.
#pragma once
#include <Arduino.h>

namespace Cleaner {
  void begin();
  void update();                       // call every loop(); self-timed
  bool start(const char *reason);      // false if a cycle is already running
  bool busy();
  uint32_t runCount();
  uint32_t lastRunMs();                // millis() when the last cycle finished, 0 if never
}
