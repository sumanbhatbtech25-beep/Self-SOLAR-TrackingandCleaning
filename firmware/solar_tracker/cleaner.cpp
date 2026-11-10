#include "cleaner.h"
#include "config.h"
#include "relays.h"
#include <ESP32Servo.h>

namespace {
  Servo servo;
  bool running = false;
  int current = WIPER_PARK_DEG;
  int waypoint = 0;                 // index into the sequence below
  uint32_t lastStep = 0;
  uint32_t runs = 0;
  uint32_t endMs = 0;
  uint32_t testMs = 0;

  // One pass = left row, back up, right row, back up.
  const int SEQ[] = {WIPER_LEFT_DEG, WIPER_PARK_DEG, WIPER_RIGHT_DEG, WIPER_PARK_DEG};
  constexpr int SEQ_LEN = sizeof(SEQ) / sizeof(SEQ[0]);
}

namespace Cleaner {

void begin() {
  servo.setPeriodHertz(50);
  servo.attach(PIN_SERVO_WIPER, 500, 2500);
  servo.write(WIPER_PARK_DEG);
}

bool busy()            { return running; }
uint32_t runCount()    { return runs; }
uint32_t lastEndMs()   { return endMs; }
uint32_t lastTestMs()  { return testMs; }

bool start() {
  if (running) return false;
  if (!Relays::servoPower()) { Relays::setServoPower(true); delay(150); }
  running = true;
  waypoint = 0;
  lastStep = millis();
  return true;
}

void moveTest(int deg) {
  if (running) return;
  if (!Relays::servoPower()) { Relays::setServoPower(true); delay(150); }
  current = constrain(deg, 0, 180);
  servo.write(current);
  testMs = millis();
}

void update() {
  if (!running || millis() - lastStep < WIPER_STEP_MS) return;
  lastStep = millis();
  int target = SEQ[waypoint % SEQ_LEN];
  if (current != target) {
    current += (target > current) ? 1 : -1;     // 1 degree per step: slow, firm sweep
    servo.write(current);
    return;
  }
  waypoint++;
  if (waypoint >= SEQ_LEN * WIPER_PASSES) {
    running = false;
    runs++;
    endMs = millis();
  }
}

}  // namespace Cleaner
