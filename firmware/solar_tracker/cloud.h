// cloud.h — ThingSpeak: every reading goes up, and the phone can send commands.
//
// Runs in its own task on core 0, so slow internet never stalls tracking.
// Two channels (a ThingSpeak channel holds 8 fields):
//
//  LIVE channel (every CLOUD_UPLOAD_MS)          HEALTH channel (every CLOUD_HEALTH_MS)
//   1 power (W)                                   1 panel temperature (C)   DS18B20
//   2 panel voltage (V)          INA219           2 air temperature (C)     DHT11
//   3 panel current (mA)         trusted sensor   3 humidity (%)            DHT11
//   4 light (mV)                 photo transistor 4 12 V supply (V)
//   5 LDR pair A (0-4095, brighter = higher)      5 INA219 current (mA)
//   6 LDR pair B                                  6 ACS712 current (mA)
//   7 tilt (deg from flat)                        7 cleanliness, current only (%)  Rule A
//   8 cleanliness (%)            Rule B           8 active faults (count)
//   status: fault codes, or "OK"                  status: mode, auto-clean, cleanings,
//                                                         self-test, last test, last command
//
// TalkBack (phone buttons) -> same commands as the Serial Monitor:
//   CLEAN  STOP  SENSOR_TEST  TILT_TEST  WIPER_TEST  SELF_TEST  FULL_TEST  CALIBRATE
//   AUTOCLEAN_ON  AUTOCLEAN_OFF  TRACK_AUTO  PARK_FLAT  UPLOAD_NOW  REBOOT
// Any other TalkBack text runs as a Serial command, e.g. "tilt 20", "label E2_s1".
#pragma once
#include <Arduino.h>

namespace Cloud {
  void begin();                // starts the task if keys are set and Wi-Fi is connected
  void snapshot();             // main loop, every second: copy the readings for the task
  void handleCommands();       // main loop: run commands that arrived from TalkBack
  void requestUpload();        // upload both channels as soon as ThingSpeak allows
  bool enabled();
  String lastResult();         // e.g. "live OK 4 s ago, health OK 31 s ago" for the dashboard
}
