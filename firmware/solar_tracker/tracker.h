// tracker.h — single-axis tracking with the tilt servo and the two LDR pairs.
#pragma once
#include <Arduino.h>

enum class TrackMode : uint8_t { Auto, Fixed, Manual };

struct TrackStatus {
  TrackMode mode = TrackMode::Auto;
  int   angle = 0;          // degrees from flat (+ towards pair A)
  float imbalance = 0;      // (A - B) / (A + B)
  bool  dark = false;
  bool  atLimit = false;
  bool  stalled = false;    // panel moved but the light balance did not change
  uint32_t lastMoveMs = 0;
};

namespace Tracker {
  void begin();
  void update(bool paused);          // call every TRACK_INTERVAL_MS
  void setMode(TrackMode m, int angle = 0);
  const TrackStatus &status();
  const char *modeName(TrackMode m);
}
