// cleaner.h — wiper servo: foam flap on a rod along the panel's centre line.
// Parked straight up (off the panels). A cycle sweeps the left row, then the
// right row, and returns to park, WIPER_PASSES times.
#pragma once
#include <Arduino.h>

namespace Cleaner {
  void begin();
  bool start();             // false if already running
  void update();            // call every loop
  bool busy();
  uint32_t runCount();
  uint32_t lastEndMs();     // millis() when the last cycle finished (0 = never)
  void moveTest(int deg);   // move to an angle for testing (not while busy)
  uint32_t lastTestMs();    // when moveTest last ran (keeps servo power on briefly)
}
