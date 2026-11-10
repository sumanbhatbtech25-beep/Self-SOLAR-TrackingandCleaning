#include "sensors.h"
#include "config.h"
#include "app_state.h"
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <Preferences.h>

namespace {
  Adafruit_INA219 ina;
  OneWire oneWire(PIN_DS18B20);
  DallasTemperature ds(&oneWire);
  DHT dht(PIN_DHT, DHT11);
  Preferences prefs;
  float zeroMv = ACS712_ZERO_MV;
  bool dsRequested = false;

  float avgMv(uint8_t pin, int n) {
    uint32_t sum = 0;
    for (int i = 0; i < n; i++) sum += analogReadMilliVolts(pin);
    return sum / float(n);
  }

  int avgRaw(uint8_t pin, int n) {
    uint32_t sum = 0;
    for (int i = 0; i < n; i++) sum += analogRead(pin);
    return sum / n;
  }

  int toLight(int raw) { return LDR_LIGHT_LOWERS_READING ? 4095 - raw : raw; }
}

namespace Sensors {

void begin() {
  for (uint8_t p : {PIN_ACS712, PIN_PHOTO, PIN_LDR_A, PIN_LDR_B, PIN_SUPPLY})
    analogSetPinAttenuation(p, ADC_11db);   // 0 to about 3.1 V
  Wire.begin(PIN_SDA, PIN_SCL);
  app.s.inaOk = ina.begin();
  ds.begin();
  ds.setWaitForConversion(false);           // never block the loop for 750 ms
  dht.begin();
  prefs.begin("solar", false);
  zeroMv = prefs.getFloat("acs0", ACS712_ZERO_MV);
}

float acsRawMv() { return avgMv(PIN_ACS712, 64) * ACS712_DIVIDER; }
float acsZero()  { return zeroMv; }

void setAcsZero(float sensorMv) {
  zeroMv = sensorMv;
  prefs.putFloat("acs0", sensorMv);
}

void readPower() {
  SensorData &s = app.s;
  // INA219: answers on I2C? A failed read returns 0, so check the bus first.
  Wire.beginTransmission(0x40);
  bool wasOk = s.inaOk;
  s.inaOk = (Wire.endTransmission() == 0);
  if (s.inaOk && !wasOk) ina.begin();       // reconnected: reload its calibration
  if (s.inaOk) {
    s.panelV = ina.getBusVoltage_V() + ina.getShuntVoltage_mV() / 1000.0f;
    s.inaA = ina.getCurrent_mA() / 1000.0f;
  } else {
    s.panelV = NAN;
    s.inaA = NAN;
  }
  // ACS712 through the divider.
  float pinMv = avgMv(PIN_ACS712, 64);
  s.acsPinMv = pinMv;
  s.acsA = (pinMv * ACS712_DIVIDER - zeroMv) / ACS712_MV_PER_A;
  // Light reference.
  s.photoMv = avgMv(PIN_PHOTO, 32);
  // Trusted current and power.
  s.currentA = (app.trusted == Sensor::INA219 && s.inaOk) ? s.inaA : s.acsA;
  s.powerW = isnan(s.panelV) ? NAN : s.panelV * s.currentA;
}

void readLdrs() {
  app.s.ldrA = toLight(avgRaw(PIN_LDR_A, 8));
  app.s.ldrB = toLight(avgRaw(PIN_LDR_B, 8));
}

void readTemps() {
  if (dsRequested) {
    float c = ds.getTempCByIndex(0);       // -127 = not connected
    app.s.panelC = c;
  }
  ds.requestTemperatures();
  dsRequested = true;
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  app.s.airC = t;                          // NAN if the DHT11 did not answer
  app.s.humidity = h;
}

void readSupply() {
  app.s.supplyV = avgMv(PIN_SUPPLY, 32) / 1000.0f * SUPPLY_DIVIDER * SUPPLY_CAL;
}

}  // namespace Sensors
