// Bring-up step 4: read the DS18B20 panel temperature probe.
// Hold the probe in your hand; the reading should rise within a few seconds.
// -127 means the probe is not found: check GPIO 4 and the 4.7 kOhm pull-up.
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire oneWire(4);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  delay(500);
  sensors.begin();
  Serial.printf("Probes found: %d\n", sensors.getDeviceCount());
}

void loop() {
  sensors.requestTemperatures();
  Serial.printf("Panel temperature: %.2f C\n", sensors.getTempCByIndex(0));
  delay(1000);
}
