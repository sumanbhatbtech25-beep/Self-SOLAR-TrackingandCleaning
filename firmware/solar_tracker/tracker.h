// tracker.h — single-axis tracking with the tilt servo and the two LDR pairs.
// Every tilt move is slewed at TILT_SLEW_MS per degree, so the board never jerks.
#pragma once
#include <Arduino.h>

enum class TrackMode : uint8_t { Auto, Fixed, Manual };

struct TrackStatus {
  TrackMode mode = TrackMode::Auto;
  int   angle = 0;          // degrees from flat (+ towards pair A), where the servo is now
  float imbalance = 0;      // (A - B) / (A + B)
  bool  dark = false;
  bool  atLimit = false;
  bool  stalled = false;    // panel moved but the light balance did not change
  uint32_t lastMoveMs = 0;
};

namespace Tracker {
  void begin();
  void update(bool paused);          // call every TRACK_INTERVAL_MS: read LDRs, choose a target
  void service();                    // call every loop: move the servo towards the target
  void setMode(TrackMode m, int angle = 0);
  void setOverride(bool on, int angle = 0);  // wiper / tests take the tilt (e.g. flat for cleaning)
  bool overridden();
  bool settled();                    // servo has reached its target
  const TrackStatus &status();
  const char *modeName(TrackMode m);
}
