// tracker.h — single-axis (east–west) sun tracking from the LDR pair.
//
// Rule: if the east LDR is brighter by more than TRACK_DEADBAND, step east;
// if the west LDR is brighter, step west; otherwise hold. At night, walk back
// to face sunrise. Tilt is in degrees from flat: negative = east, positive = west.
#pragma once
#include <Arduino.h>

enum class TrackerMode  { Auto, Manual };
enum class TrackerState { Tracking, Holding, AtLimit, NightPark, Manual, Paused };

struct TrackerStatus {
  int   tiltDeg         = 0;
  TrackerMode  mode     = TrackerMode::Auto;
  TrackerState state    = TrackerState::Holding;
  int   ldrEast         = 0;
  int   ldrWest         = 0;
  float imbalance       = 0;     // (E−W)/(E+W), positive = sun is east of the panel
  bool  stallSuspected  = false; // moving for a while but imbalance not shrinking
};

namespace Tracker {
  void begin(int initialTiltDeg = 0);   // restore the pre-sleep position
  void update();                      // call every loop(); self-timed
  void setPaused(bool paused);        // used while the wiper runs
  void setMode(TrackerMode mode);
  void setManualTilt(int deg);        // switches to Manual mode
  const TrackerStatus &status();
  const char *stateName(TrackerState s);
  const char *modeName(TrackerMode m);
}
