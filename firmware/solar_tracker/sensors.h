// sensors.h — INA219 power sensor, DS18B20 panel temperature and the LDR pair.
#pragma once
#include <Arduino.h>

struct SensorData {
  float busVoltage   = 0;     // V, panel voltage at the load (INA219 VIN− to GND)
  float current      = 0;     // A
  float power        = 0;     // W
  float panelTempC   = NAN;   // °C, NAN if the probe is missing
  int   ldrEast      = 0;     // 0–4095
  int   ldrWest      = 0;     // 0–4095
  bool  inaOk        = false;
  bool  tempOk       = false;
  uint32_t timestamp = 0;     // millis() of this reading
};

namespace Sensors {
  bool begin();                 // returns false if the INA219 is not found
  void readLdrs(int &east, int &west);   // fast, used by the tracker every step
  void update(SensorData &out); // full read, call every SENSOR_INTERVAL_MS
}
