#include "cleaner.h"
#include "config.h"
#include <ESP32Servo.h>

namespace {
  enum class Phase { Idle, Out, Back, Settle };

  Servo    wiper;
  Phase    phase      = Phase::Idle;
  int      pos        = WIPER_PARK_DEG;
  int      passesDone = 0;
  uint32_t lastStep   = 0;
  uint32_t settleAt   = 0;
  uint32_t runs       = 0;
  uint32_t lastRun    = 0;

  // Step pos toward target; returns true once it arrives.
  bool stepToward(int target) {
    if (pos < target) pos = min(pos + WIPER_STEP_DEG, target);
    else if (pos > target) pos = max(pos - WIPER_STEP_DEG, target);
    wiper.write(pos);
    return pos == target;
  }
}

namespace Cleaner {

void begin() {
  wiper.setPeriodHertz(50);
  // Park once at boot, then detach so the idle servo doesn't buzz or draw current.
  wiper.attach(PIN_SERVO_WIPER, SERVO_MIN_US, SERVO_MAX_US);
  wiper.write(WIPER_PARK_DEG);
  delay(400);
  wiper.detach();
}

bool start(const char *reason) {
  if (phase != Phase::Idle) return false;
  (void)reason;
  wiper.attach(PIN_SERVO_WIPER, SERVO_MIN_US, SERVO_MAX_US);
  pos = WIPER_PARK_DEG;
  wiper.write(pos);
  passesDone = 0;
  phase = Phase::Out;
  lastStep = millis();
  return true;
}

void update() {
  if (phase == Phase::Idle) return;
  uint32_t now = millis();

  if (phase == Phase::Settle) {
    if (now >= settleAt) {
      wiper.detach();
      phase = Phase::Idle;
      runs++;
      lastRun = now;
    }
    return;
  }

  if (now - lastStep < WIPER_STEP_MS) return;
  lastStep = now;

  if (phase == Phase::Out) {
    if (stepToward(WIPER_END_DEG)) phase = Phase::Back;
  } else if (phase == Phase::Back) {
    if (stepToward(WIPER_PARK_DEG)) {
      if (++passesDone >= WIPER_PASSES) {
        phase = Phase::Settle;
        settleAt = now + 300;   // let the blade reach park before detaching
      } else {
        phase = Phase::Out;
      }
    }
  }
}

bool busy()          { return phase != Phase::Idle; }
uint32_t runCount()  { return runs; }
uint32_t lastRunMs() { return lastRun; }

}  // namespace Cleaner
