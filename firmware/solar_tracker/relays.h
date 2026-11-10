// relays.h — the 2-channel relay module.
// Relay 1 (GPIO 25): OFF = panels drive the 4.4 ohm load, ON = panels disconnected.
// Relay 2 (GPIO 26): ON = 12 V reaches Buck B, so the servos have power.
#pragma once
#include <Arduino.h>

namespace Relays {
  void begin();                     // both relays OFF; call first in setup()
  void setPanelConnected(bool on);  // true = panels drive the load (relay 1 OFF)
  bool panelConnected();
  void setServoPower(bool on);
  bool servoPower();
}
