// Bring-up step 7: read panel voltage, current and power.
// Compare the voltage against a multimeter across the load resistor.
#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina(0x40);

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);
  if (!ina.begin()) {
    Serial.println("INA219 not found. Run tests/i2c_scan first.");
    while (true) delay(1000);
  }
  ina.setCalibration_32V_2A();
  Serial.println("voltage_v,current_ma,power_mw");
}

void loop() {
  float v  = ina.getBusVoltage_V();
  float ma = ina.getCurrent_mA();
  Serial.printf("%.2f,%.1f,%.1f\n", v, ma, v * ma);
  delay(1000);
}
