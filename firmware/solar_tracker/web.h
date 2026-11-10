// web.h — Wi-Fi, the dashboard page and its small JSON API.
//   GET  /             dashboard
//   GET  /api/status   all readings as JSON
//   POST /api/cmd?c=…  run a command (same as the Serial Monitor, e.g. c=clean)
#pragma once
#include <Arduino.h>

namespace Web {
  void begin();
  void handle();
  String address();
}
