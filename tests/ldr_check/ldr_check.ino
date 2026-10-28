// Bring-up step 6: check both LDR dividers.
// Cover one LDR with your finger: its number should drop sharply.
// In full sun the readings should stay below ~4000. If they sit at 4095,
// swap the 10 kOhm divider resistors for 4.7 kOhm or 2.2 kOhm.
const int PIN_EAST = 34;
const int PIN_WEST = 35;

int readAvg(int pin) {
  long sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(pin);
  return sum / 16;
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_EAST, ADC_11db);
  analogSetPinAttenuation(PIN_WEST, ADC_11db);
}

void loop() {
  int e = readAvg(PIN_EAST), w = readAvg(PIN_WEST);
  float imbalance = (e + w) > 0 ? float(e - w) / (e + w) : 0;
  Serial.printf("east=%4d  west=%4d  imbalance=%+.3f  -> %s\n", e, w, imbalance,
                imbalance > 0.04 ? "turn EAST" : imbalance < -0.04 ? "turn WEST" : "hold");
  delay(300);
}
