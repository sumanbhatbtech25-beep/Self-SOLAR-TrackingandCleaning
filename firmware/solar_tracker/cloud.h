// cloud.h — ThingSpeak upload and TalkBack remote commands.
//
// Runs in its own task on core 0, so slow internet never stalls tracking.
// Channel fields:
//   1 panel power (W)      2 panel current (mA)   3 light (mV)       4 cleanliness (%)
//   5 tilt angle (deg)     6 panel temp (C)       7 air temp (C)     8 humidity (%)
//   status: active fault codes, or "OK"
// TalkBack commands (phone buttons): CLEAN, AUTOCLEAN_ON, AUTOCLEAN_OFF, SELF_TEST,
//   CALIBRATE, TRACK_AUTO, PARK_FLAT
#pragma once
#include <Arduino.h>

namespace Cloud {
  void begin();                // starts the task if keys are set and Wi-Fi is connected
  void handleCommands();       // main loop: run commands that arrived from TalkBack
  bool enabled();
  String lastResult();         // e.g. "upload OK 12:03:41 ago 8 s" for the dashboard
}
