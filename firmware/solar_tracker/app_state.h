// app_state.h — shared state between the control loop and the web server.
#pragma once
#include <Arduino.h>
#include "sensors.h"
#include "diagnostics.h"
#include "power.h"

// Small ring buffer of human-readable events shown on the dashboard.
struct EventLog {
  static constexpr int N = 10;
  char lines[N][96];
  int  head  = 0;
  int  count = 0;
  void add(const char *fmt, ...);
  // i = 0 is the newest line
  const char *get(int i) const { return lines[(head - 1 - i + N) % N]; }
};

struct AppState {
  SensorData sensors;
  BatteryData battery;
  DiagReport diag;
  EventLog   log;
  bool       autoClean     = false;
  bool       stationMode   = false;   // true = joined your Wi-Fi, false = own access point
  IPAddress  ip;
};

extern AppState app;
