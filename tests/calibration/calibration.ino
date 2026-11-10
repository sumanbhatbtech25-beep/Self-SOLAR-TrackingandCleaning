// Calibration sketch: find the servo angles and see the raw light readings.
// Serial Monitor: 115200 baud, line ending "Newline".
//   t<angle>   move the tilt servo,  e.g. t90
//   w<angle>   move the wiper servo, e.g. w170
// Every second it prints both LDR pairs, the photo transistor, the ACS712 pin
// voltage and the 12 V supply. Copy the numbers into firmware/solar_tracker/config.h:
//   TILT_FLAT_DEG, WIPER_PARK_DEG, WIPER_LEFT_DEG, WIPER_RIGHT_DEG,
//   LDR_LIGHT_LOWERS_READING (does an LDR reading go DOWN when you shine the torch?),
//   LDR_DARK_LEVEL (about half the room-light reading, after flipping if needed).
#include <ESP32Servo.h>

Servo tilt, wiper;

void setup() {
  Serial.begin(115200);
  digitalWrite(25, HIGH); pinMode(25, OUTPUT);   // relay 1 OFF: panels drive the load
  digitalWrite(26, HIGH); pinMode(26, OUTPUT);   // relay 2 OFF for now
  for (int p : {34, 35, 36, 39, 33}) analogSetPinAttenuation(p, ADC_11db);
  tilt.setPeriodHertz(50);  tilt.attach(18, 500, 2500);
  wiper.setPeriodHertz(50); wiper.attach(19, 500, 2500);
  tilt.write(90);
  wiper.write(90);
  delay(300);
  digitalWrite(26, LOW);                         // relay 2 ON: servo power
  Serial.println("Type t<angle> for tilt or w<angle> for wiper, e.g. t90 or w170");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't' || c == 'w') {
      int a = constrain(Serial.parseInt(), 0, 180);
      if (c == 't') tilt.write(a); else wiper.write(a);
      Serial.printf("%s -> %d\n", c == 't' ? "TILT" : "WIPER", a);
    }
  }
  static uint32_t last = 0;
  if (millis() - last > 1000) {
    last = millis();
    Serial.printf("LDR_A(34)=%4d  LDR_B(35)=%4d  PHOTO(39)=%4u mV  ACS712 pin=%4u mV  supply=%.2f V\n",
                  analogRead(34), analogRead(35), analogReadMilliVolts(39),
                  analogReadMilliVolts(36), analogReadMilliVolts(33) / 1000.0 * 4.13);
  }
}
