#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>

namespace {
  Adafruit_INA219   ina(INA219_ADDRESS);
  OneWire           oneWire(PIN_DS18B20);
  DallasTemperature dallas(&oneWire);
  bool inaFound = false;
  bool tempConversionPending = false;

  int readAveraged(uint8_t pin) {
    uint32_t sum = 0;
    for (int i = 0; i < LDR_SAMPLES; i++) sum += analogRead(pin);
    return static_cast<int>(sum / LDR_SAMPLES);
  }
}

namespace Sensors {

bool begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_LDR_EAST, ADC_11db);   // full 0–3.3 V range
  analogSetPinAttenuation(PIN_LDR_WEST, ADC_11db);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  inaFound = ina.begin();
  if (inaFound) {
    // 32 V / 2 A range suits a 5–10 W, ~21 V Voc panel.
    ina.setCalibration_32V_2A();
  }

  dallas.begin();
  dallas.setResolution(11);              // 0.125 °C, ~375 ms conversion
  dallas.setWaitForConversion(false);    // never block the loop
  dallas.requestTemperatures();
  tempConversionPending = true;

  return inaFound;
}

void readLdrs(int &east, int &west) {
  east = readAveraged(PIN_LDR_EAST);
  west = readAveraged(PIN_LDR_WEST);
}

void update(SensorData &out) {
  out.timestamp = millis();
  out.inaOk = inaFound;

  if (inaFound) {
    out.busVoltage = ina.getBusVoltage_V();
    out.current    = ina.getCurrent_mA() / 1000.0f;
    if (out.current < 0) out.current = 0;           // ignore tiny negative offsets
    out.power      = out.busVoltage * out.current;
  } else {
    out.busVoltage = out.current = out.power = 0;
  }

  // The previous conversion was requested one interval ago, so it is ready now.
  if (tempConversionPending) {
    float t = dallas.getTempCByIndex(0);
    out.tempOk = (t != DEVICE_DISCONNECTED_C && t > -55 && t < 125);
    out.panelTempC = out.tempOk ? t : NAN;
  }
  dallas.requestTemperatures();
  tempConversionPending = true;

  readLdrs(out.ldrEast, out.ldrWest);
}

}  // namespace Sensors
