// sensors.h — reads every sensor into app.s.
#pragma once
#include <Arduino.h>

namespace Sensors {
  void begin();
  void readPower();    // INA219, ACS712, photo transistor (every second)
  void readLdrs();     // both LDR pairs (every tracking step)
  void readTemps();    // DS18B20 and DHT11 (slow)
  void readSupply();   // 12 V supply
  float acsRawMv();    // ACS712 output voltage at the sensor (mV), averaged
  void setAcsZero(float sensorMv);  // save a new zero point (measured with no current)
  float acsZero();
}
