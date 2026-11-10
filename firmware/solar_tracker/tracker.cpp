#include "tracker.h"
#include "config.h"
#include "app_state.h"
#include "relays.h"
#include "sensors.h"
#include <ESP32Servo.h>

namespace {
  Servo servo;
  TrackStatus st;
  int  target = 0;          // where the tilt is heading (degrees from flat)
  int  modeAngle = 0;       // the angle held in Fixed / Manual mode
  bool ovr = false;         // the wiper or a test has taken over the tilt
  int  ovrAngle = 0;
  uint32_t lastSlew = 0;
  uint32_t powerOnMs = 0;
  uint32_t darkSince = 0;
  // Stall check: where the current off-target run started.
  bool  runActive = false;
  int   runAngle0 = 0;
  float runImb0 = 0;

  int clampTilt(int a) { return constrain(a, -TILT_RANGE_DEG, TILT_RANGE_DEG); }

  void writeServo(int angle) { servo.write(constrain(TILT_FLAT_DEG + angle, 0, 180)); }

  // Set a new target; service() moves there one degree at a time.
  void aim(int angle) {
    target = clampTilt(angle);
    st.atLimit = abs(target) >= TILT_RANGE_DEG;
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
  writeServo(0);
  st.angle = 0;
  target = 0;
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
  if (m != TrackMode::Auto) {
    modeAngle = clampTilt(angle);
    if (!ovr) aim(modeAngle);
  }
}

void setOverride(bool on, int angle) {
  ovr = on;
  ovrAngle = clampTilt(angle);
  runActive = false;
  st.stalled = false;
  if (on) {
    aim(ovrAngle);
  } else if (st.mode != TrackMode::Auto) {
    aim(modeAngle);                 // back to the fixed / manual angle
  } else {
    aim(st.angle);                  // auto: tracking carries on from here
  }
}

bool overridden() { return ovr; }
bool settled()    { return st.angle == target; }

void service() {
  if (st.angle == target || millis() - lastSlew < TILT_SLEW_MS) return;
  if (!Relays::servoPower()) {      // normal mode: power up, then wait 150 ms before moving
    Relays::setServoPower(true);
    powerOnMs = millis();
    st.lastMoveMs = millis();
    return;
  }
  if (millis() - powerOnMs < 150) return;
  lastSlew = millis();
  st.angle += (target > st.angle) ? 1 : -1;
  writeServo(st.angle);
  st.lastMoveMs = millis();
}

void update(bool paused) {
  Sensors::readLdrs();
  int a = app.s.ldrA, b = app.s.ldrB;
  int sum = a + b;
  st.imbalance = sum > 0 ? float(a - b) / sum : 0;
  st.dark = sum < LDR_DARK_LEVEL;

  if (st.mode != TrackMode::Auto || paused || ovr) return;

  if (st.dark) {
    runActive = false;
    if (!DEMO_MODE) {
      if (darkSince == 0) darkSince = millis();
      if (millis() - darkSince > NIGHT_CONFIRM_MS) aim(0);    // night: park flat
    }
    return;                                                   // demo: just hold still
  }
  darkSince = 0;

  checkStall(st.imbalance);
  if (fabsf(st.imbalance) < TRACK_DEADBAND) return;
  int step = (DEMO_MODE && fabsf(st.imbalance) > TRACK_FAST_ABOVE) ? 2 : 1;
  int dir = (st.imbalance > 0) ? 1 : -1;
  if (TRACK_INVERT) dir = -dir;
  aim(st.angle + dir * step);
}

}  // namespace Tracker
