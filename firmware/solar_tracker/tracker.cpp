#include "tracker.h"
#include "config.h"
#include "app_state.h"
#include "relays.h"
#include "sensors.h"
#include <ESP32Servo.h>

namespace {
  Servo servo;
  TrackStatus st;
  uint32_t darkSince = 0;
  // Stall check: where the current off-target run started.
  bool  runActive = false;
  int   runAngle0 = 0;
  float runImb0 = 0;

  void moveTo(int angle) {
    angle = constrain(angle, -TILT_RANGE_DEG, TILT_RANGE_DEG);
    st.atLimit = (abs(angle) >= TILT_RANGE_DEG);
    if (angle == st.angle) return;
    if (!Relays::servoPower()) {          // normal mode: power up just before moving
      Relays::setServoPower(true);
      delay(150);
    }
    st.angle = angle;
    servo.write(TILT_FLAT_DEG + angle);
    st.lastMoveMs = millis();
  }

  void checkStall(float imb) {
    if (fabsf(imb) < TRACK_DEADBAND) { runActive = false; st.stalled = false; return; }
    if (!runActive) { runActive = true; runAngle0 = st.angle; runImb0 = imb; return; }
    if (abs(st.angle - runAngle0) >= TILT_STALL_DEG) {
      // The panel turned towards the light; a working tracker reduces the imbalance.
      st.stalled = fabsf(imb) > TILT_STALL_SHRINK * fabsf(runImb0);
      runAngle0 = st.angle;
      runImb0 = imb;
    }
  }
}

namespace Tracker {

void begin() {
  servo.setPeriodHertz(50);
  servo.attach(PIN_SERVO_TILT, 500, 2500);
  servo.write(TILT_FLAT_DEG);
  st.angle = 0;
}

const TrackStatus &status() { return st; }

const char *modeName(TrackMode m) {
  switch (m) {
    case TrackMode::Auto:  return "auto";
    case TrackMode::Fixed: return "fixed";
    default:               return "manual";
  }
}

void setMode(TrackMode m, int angle) {
  st.mode = m;
  runActive = false;
  st.stalled = false;
  if (m != TrackMode::Auto) moveTo(angle);
}

void update(bool paused) {
  Sensors::readLdrs();
  int a = app.s.ldrA, b = app.s.ldrB;
  int sum = a + b;
  st.imbalance = sum > 0 ? float(a - b) / sum : 0;
  st.dark = sum < LDR_DARK_LEVEL;

  if (st.mode != TrackMode::Auto || paused) return;

  if (st.dark) {
    runActive = false;
    if (!DEMO_MODE) {
      if (darkSince == 0) darkSince = millis();
      if (millis() - darkSince > NIGHT_CONFIRM_MS) moveTo(0);   // night: park flat
    }
    return;                                                     // demo: just hold still
  }
  darkSince = 0;

  checkStall(st.imbalance);
  if (fabsf(st.imbalance) < TRACK_DEADBAND) return;
  int step = (DEMO_MODE && fabsf(st.imbalance) > TRACK_FAST_ABOVE) ? 2 : 1;
  int dir = (st.imbalance > 0) ? 1 : -1;
  if (TRACK_INVERT) dir = -dir;
  moveTo(st.angle + dir * step);
}

}  // namespace Tracker
