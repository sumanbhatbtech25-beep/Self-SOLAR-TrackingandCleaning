// web.h — Wi-Fi dashboard and JSON API (see README "Dashboard and API").
#pragma once

namespace Web {
  void begin();    // registers routes and starts the server on port 80
  void handle();   // call every loop()
}
