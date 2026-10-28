// Bring-up step 4: check the battery voltage divider on GPIO 32.
// Measure the battery with a multimeter at the same time. If the two differ,
// set BATTERY_CAL in config.h to  multimeter_reading / printed_reading.
// Example: multimeter 12.70 V, printed 12.46 V  ->  BATTERY_CAL = 1.02
const int   PIN_BATTERY = 32;
const float DIVIDER     = (100.0f + 27.0f) / 27.0f;   // 100 kΩ top, 27 kΩ bottom

void setup() {
  Serial.begin(115200);
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);
}

void loop() {
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(PIN_BATTERY);
  float pinV = mv / 16.0f / 1000.0f;
  float batV = pinV * DIVIDER;
  Serial.printf("pin %.3f V  ->  battery %.2f V  %s\n", pinV, batV,
                batV < 5 ? "(divider not connected?)" :
                batV < 11.8 ? "(flat)" : batV < 12.4 ? "(low)" : batV < 13.6 ? "(ok)" : "(charging / full)");
  delay(1000);
}
