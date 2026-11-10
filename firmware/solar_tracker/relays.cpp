#include "relays.h"
#include "config.h"

namespace {
  bool panelOn = true;
  bool servoOn = false;

  void drive(uint8_t pin, bool relayOn) {
    digitalWrite(pin, (relayOn == RELAY_ACTIVE_LOW) ? LOW : HIGH);
  }
}

namespace Relays {

void begin() {
  // Set the level before switching to output so the relays never click at boot.
  drive(PIN_RELAY_LOAD, false);
  drive(PIN_RELAY_SERVO, false);
  pinMode(PIN_RELAY_LOAD, OUTPUT);
  pinMode(PIN_RELAY_SERVO, OUTPUT);
  panelOn = true;
  servoOn = false;
}

void setPanelConnected(bool on) { panelOn = on; drive(PIN_RELAY_LOAD, !on); }
bool panelConnected()           { return panelOn; }
void setServoPower(bool on)     { servoOn = on; drive(PIN_RELAY_SERVO, on); }
bool servoPower()               { return servoOn; }

}  // namespace Relays
