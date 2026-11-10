// cleaner.h — wiper servo: foam flap on a rod along the panel's centre line.
// Parked straight up (off the panels). A cycle first brings the panels flat
// (CLEAN_AT_FLAT), then sweeps the left row and the right row, returning to
// park after each, WIPER_PASSES times. Tracking resumes afterwards.
#pragma once
#include <Arduino.h>
#include "config.h"

namespace Cleaner {
  void begin();
  bool start(int passes = WIPER_PASSES);  // false if already running
  void stop();              // end the cycle: the foam returns to park
  void update();            // call every loop
  bool busy();
  uint32_t runCount();
  uint32_t lastEndMs();     // millis() when the last cycle finished (0 = never)
  void moveTest(int deg);   // move to an angle for testing (not while busy)
  uint32_t lastTestMs();    // when moveTest last ran (keeps servo power on briefly)
}
