// Bring-up step 3: find the INA219 on the I2C bus.
// Expected output: "Found device at 0x40". Nothing found = check SDA/SCL and 3V3.
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);   // SDA, SCL
}

void loop() {
  int found = 0;
  Serial.println("Scanning I2C...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) Serial.println("  No devices found. Check wires 12-15.");
  delay(3000);
}
