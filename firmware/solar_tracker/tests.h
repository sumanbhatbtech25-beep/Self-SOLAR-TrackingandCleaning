// tests.h — on-demand tests, started from the phone (ThingSpeak TalkBack), the
// dashboard or the Serial Monitor ("test sensors|tilt|wiper|full", "stop").
//
//   sensors  every sensor checked once: INA219, ACS712, photo transistor, both LDR
//            pairs, DS18B20, DHT11, 12 V supply                      (instant)
//   tilt     panel slewed to +limit, -limit and back to flat; the LDR balance
//            must change between the two ends                        (~15 s)
//   wiper    one slow wiper pass (panel levelled first)              (~15 s)
//   full     sensors -> current-sensor self-test (relay) -> tilt -> wiper
//
// The one-line result goes to app.testResult: dashboard, ThingSpeak status, Serial.
#pragma once
#include <Arduino.h>

namespace Tests {
  bool start(const String &which, String &msg);
  void stop();
  void update();              // call every loop
  bool busy();                // a tilt / wiper / full test is running
}
