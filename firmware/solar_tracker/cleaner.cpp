#include "cleaner.h"
#include "config.h"
#include "relays.h"
#include "tracker.h"
#include <ESP32Servo.h>

namespace {
  Servo servo;
  enum class Phase : uint8_t { Idle, Level, Settle, Sweep };
  Phase phase = Phase::Idle;
  int current = WIPER_PARK_DEG;
  int waypoint = 0;                 // index into the sequence below
  int passes = WIPER_PASSES;
  uint32_t lastStep = 0;
  uint32_t phaseMs = 0;
  uint32_t runs = 0;
  uint32_t endMs = 0;
  uint32_t testMs = 0;

  // One pass = left row, back up, right row, back up. The last entry must be PARK.
  const int SEQ[] = {WIPER_LEFT_DEG, WIPER_PARK_DEG, WIPER_RIGHT_DEG, WIPER_PARK_DEG};
  constexpr int SEQ_LEN = sizeof(SEQ) / sizeof(SEQ[0]);

  void finish(bool completed) {
    phase = Phase::Idle;
    if (completed) runs++;
    endMs = millis();
    if (CLEAN_AT_FLAT) Tracker::setOverride(false);   // tracking / fixed angle resumes
  }
}

namespace Cleaner {

void begin() {
  servo.setPeriodHertz(50);
  servo.attach(PIN_SERVO_WIPER, 500, 2500);
  servo.write(WIPER_PARK_DEG);
}

bool busy()            { return phase != Phase::Idle; }
uint32_t runCount()    { return runs; }
uint32_t lastEndMs()   { return endMs; }
uint32_t lastTestMs()  { return testMs; }

bool start(int n) {
  if (busy()) return false;
  passes = constrain(n, 1, 5);
  if (!Relays::servoPower()) { Relays::setServoPower(true); delay(150); }
  waypoint = 0;
  lastStep = millis();
  phaseMs = millis();
  if (CLEAN_AT_FLAT) {
    // The wiper is on a fixed post: bring the panels flat under it first.
    Tracker::setOverride(true, 0);
    phase = Phase::Level;
  } else {
    phase = Phase::Sweep;
  }
  return true;
}

void stop() {
  if (phase == Phase::Level || phase == Phase::Settle) { finish(false); return; }
  // Sweeping: jump to the last waypoint of the cycle (PARK). The foam rises off the
  // panel at the normal speed and the cycle ends there.
  if (phase == Phase::Sweep) waypoint = SEQ_LEN * passes - 1;
}

void moveTest(int deg) {
  if (busy()) return;
  if (!Relays::servoPower()) { Relays::setServoPower(true); delay(150); }
  current = constrain(deg, 0, 180);
  servo.write(current);
  testMs = millis();
}

void update() {
  switch (phase) {
    case Phase::Idle:
      return;
    case Phase::Level:
      if (Tracker::settled()) { phase = Phase::Settle; phaseMs = millis(); }
      return;
    case Phase::Settle:                              // let the board stop swinging
      if (millis() - phaseMs >= 400) { phase = Phase::Sweep; lastStep = millis(); }
      return;
    case Phase::Sweep:
      break;
  }
  if (millis() - lastStep < WIPER_STEP_MS) return;
  lastStep = millis();
  int target = SEQ[waypoint % SEQ_LEN];
  if (current != target) {
    current += (target > current) ? 1 : -1;     // 1 degree per step: slow, firm sweep
    servo.write(current);
    return;
  }
  waypoint++;
  if (waypoint >= SEQ_LEN * passes) finish(true);
}

}  // namespace Cleaner
