#include "tracker.h"
#include "config.h"
#include "sensors.h"
#include <ESP32Servo.h>

namespace {
  Servo tiltServo;
  TrackerStatus st;
  bool     paused      = false;
  uint32_t lastStep    = 0;
  int      nightCount  = 0;
  bool     moving      = false;
  uint32_t moveStart   = 0;
  float    moveStartImbalance = 0;

  void writeTilt(int deg) {
    deg = constrain(deg, -TILT_LIMIT_DEG, TILT_LIMIT_DEG);
    st.tiltDeg = deg;
    int angle = TILT_SERVO_CENTER + (TILT_INVERT ? -deg : deg);
    tiltServo.write(constrain(angle, 0, 180));
  }

  void stopMoving() {
    moving = false;
    st.stallSuspected = false;
  }
}

namespace Tracker {

void begin() {
  tiltServo.setPeriodHertz(50);
  tiltServo.attach(PIN_SERVO_TILT, SERVO_MIN_US, SERVO_MAX_US);
  writeTilt(0);   // start flat
}

void update() {
  uint32_t now = millis();
  if (now - lastStep < TRACK_INTERVAL_MS) return;
  lastStep = now;

  Sensors::readLdrs(st.ldrEast, st.ldrWest);
  int sum = st.ldrEast + st.ldrWest;
  st.imbalance = sum > 0 ? float(st.ldrEast - st.ldrWest) / float(sum) : 0.0f;

  if (paused)                       { st.state = TrackerState::Paused; stopMoving(); return; }
  if (st.mode == TrackerMode::Manual) { st.state = TrackerState::Manual; stopMoving(); return; }

  // Night: slowly return to face sunrise so the morning starts aligned.
  if (sum / 2 < DARK_THRESHOLD) {
    st.state = TrackerState::NightPark;
    stopMoving();
    if (++nightCount >= NIGHT_PARK_DIVIDER) {
      nightCount = 0;
      if (st.tiltDeg > -TILT_LIMIT_DEG) writeTilt(st.tiltDeg - TILT_STEP_DEG);
    }
    return;
  }

  int dir = 0;
  if (st.imbalance >  TRACK_DEADBAND) dir = -1;   // east brighter -> turn east
  if (st.imbalance < -TRACK_DEADBAND) dir = +1;   // west brighter -> turn west

  if (dir == 0) { st.state = TrackerState::Holding; stopMoving(); return; }

  int target = st.tiltDeg + dir * TILT_STEP_DEG;
  if (target < -TILT_LIMIT_DEG || target > TILT_LIMIT_DEG) {
    st.state = TrackerState::AtLimit;   // sun is lower than the frame can follow
    stopMoving();
    return;
  }

  // Stall check: with no position feedback, a servo that isn't turning shows up
  // as an imbalance that refuses to shrink while we keep commanding moves.
  float absImb = fabsf(st.imbalance);
  if (!moving) {
    moving = true; moveStart = now; moveStartImbalance = absImb;
  } else if (now - moveStart >= DIAG_STALL_MS) {
    st.stallSuspected = absImb > 0.8f * moveStartImbalance;
    moveStart = now; moveStartImbalance = absImb;
  }

  writeTilt(target);
  st.state = TrackerState::Tracking;
}

void setPaused(bool p) { paused = p; }

void setMode(TrackerMode m) {
  st.mode = m;
  stopMoving();
}

void setManualTilt(int deg) {
  st.mode = TrackerMode::Manual;
  stopMoving();
  writeTilt(deg);
}

const TrackerStatus &status() { return st; }

const char *stateName(TrackerState s) {
  switch (s) {
    case TrackerState::Tracking:  return "tracking";
    case TrackerState::Holding:   return "holding";
    case TrackerState::AtLimit:   return "at_limit";
    case TrackerState::NightPark: return "night";
    case TrackerState::Manual:    return "manual";
    case TrackerState::Paused:    return "paused";
  }
  return "unknown";
}

const char *modeName(TrackerMode m) { return m == TrackerMode::Auto ? "auto" : "manual"; }

}  // namespace Tracker
