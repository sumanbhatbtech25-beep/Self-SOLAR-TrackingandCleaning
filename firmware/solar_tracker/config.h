// config.h — every pin, calibration value and threshold in one place.
// Values marked CALIBRATE come from tests/calibration on your own rig.
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Mode
// ---------------------------------------------------------------------------
// true  = torch demo: tracking reacts in real time, checks run every few seconds,
//         servo power stays on, no night parking.
// false = normal outdoor running: slow, energy-saving gaps (see INTERVALS below).
constexpr bool DEMO_MODE = true;

// ---------------------------------------------------------------------------
// Pins (as built, see docs/images/wiring-schematic.png)
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_SDA          = 21;  // INA219
constexpr uint8_t PIN_SCL          = 22;
constexpr uint8_t PIN_ACS712       = 36;  // VP, through a 10k / 10k divider
constexpr uint8_t PIN_PHOTO        = 39;  // VN, photo transistor emitter + 10k to GND
constexpr uint8_t PIN_LDR_A        = 34;  // LDR pair A (top edge), module AO pins joined
constexpr uint8_t PIN_LDR_B        = 35;  // LDR pair B (bottom edge)
constexpr uint8_t PIN_SUPPLY       = 33;  // 12 V supply divider (21.3k + 10k / 10k)
constexpr uint8_t PIN_RELAY_LOAD   = 25;  // relay IN1: panels -> 4.4 ohm load (OFF) / open (ON)
constexpr uint8_t PIN_RELAY_SERVO  = 26;  // relay IN2: 12 V -> Buck B (servo power)
constexpr uint8_t PIN_SERVO_TILT   = 18;
constexpr uint8_t PIN_SERVO_WIPER  = 19;
constexpr uint8_t PIN_DS18B20      = 4;   // panel temperature, 10k pull-up
constexpr uint8_t PIN_DHT          = 23;  // air temperature + humidity (DHT11)

constexpr bool RELAY_ACTIVE_LOW = true;   // most 2-channel modules: LOW = relay ON

// ---------------------------------------------------------------------------
// Servo calibration (CALIBRATE with tests/calibration: t<angle> / w<angle>)
// ---------------------------------------------------------------------------
constexpr int  TILT_FLAT_DEG     = 90;    // servo angle where the panels lie flat
constexpr int  TILT_RANGE_DEG    = 60;    // tracking limit either side of flat
constexpr bool TRACK_INVERT      = false; // set true if the panel turns AWAY from the torch

constexpr int  WIPER_PARK_DEG    = 90;    // foam straight up, off the panels
constexpr int  WIPER_LEFT_DEG    = 10;    // foam pressing on the left row
constexpr int  WIPER_RIGHT_DEG   = 170;   // foam pressing on the right row
constexpr int  WIPER_PASSES      = 2;
constexpr uint32_t WIPER_STEP_MS = 15;    // ms per degree: higher = slower sweep

// ---------------------------------------------------------------------------
// Sensor scaling
// ---------------------------------------------------------------------------
constexpr float ACS712_MV_PER_A   = 185.0f;  // 5 A version
constexpr float ACS712_DIVIDER    = 2.0f;    // 10k / 10k
constexpr float ACS712_ZERO_MV    = 2500.0f; // starting guess; re-measured at boot and in every self-test
constexpr float SUPPLY_DIVIDER    = 4.13f;   // (21.3k + 10k + 10k) / 10k
constexpr float SUPPLY_CAL        = 1.00f;   // multimeter reading / dashboard reading
constexpr float TEST_LOAD_OHMS    = 4.4f;    // 5 x 22 ohm in parallel

// LDR modules: on most modules the AO voltage FALLS as light rises.
// CALIBRATE: shine the torch on one pair; if its reading goes up, set false.
constexpr bool LDR_LIGHT_LOWERS_READING = true;
constexpr int  LDR_DARK_LEVEL     = 300;     // CALIBRATE: summed light (both pairs) below this = dark
constexpr float PHOTO_SATURATED_MV = 3050.0f; // photo transistor near its ceiling: dust check paused
constexpr float PHOTO_MIN_MV      = 150.0f;  // too dark for a meaningful dust check

// ---------------------------------------------------------------------------
// Intervals (ms). Demo values are short so results show up during a demo.
// ---------------------------------------------------------------------------
constexpr uint32_t LOG_INTERVAL_MS      = 1000;                        // CSV line + power read
constexpr uint32_t TRACK_INTERVAL_MS    = DEMO_MODE ? 50     : 60000;
constexpr uint32_t TEMP_INTERVAL_MS     = DEMO_MODE ? 5000   : 120000; // DHT11 needs >= 2 s
constexpr uint32_t SUPPLY_INTERVAL_MS   = DEMO_MODE ? 5000   : 60000;
constexpr uint32_t SOILING_INTERVAL_MS  = DEMO_MODE ? 5000   : 300000;
constexpr int      SOILING_CONFIRM      = 3;      // low results in a row before SOILING is raised
constexpr uint32_t AUTO_CLEAN_GAP_MS    = DEMO_MODE ? 60000  : 7200000UL;
constexpr uint32_t NIGHT_CONFIRM_MS     = 15UL * 60 * 1000;            // normal mode only
constexpr uint32_t SELFTEST_EVERY_MS    = 24UL * 3600 * 1000;
constexpr uint32_t SERVO_HOLD_MS        = 1000;   // normal mode: power stays on this long after a move
constexpr uint32_t CLOUD_UPLOAD_MS      = DEMO_MODE ? 20000  : 120000; // ThingSpeak free plan: 15 s minimum
constexpr uint32_t CLOUD_TALKBACK_MS    = DEMO_MODE ? 5000   : 30000;  // how often phone commands are checked

// ---------------------------------------------------------------------------
// Tracking
// ---------------------------------------------------------------------------
constexpr float TRACK_DEADBAND   = 0.05f;  // |A-B|/(A+B) below this: hold still
constexpr float TRACK_FAST_ABOVE = 0.20f;  // demo: 2 degree steps when this far off

// ---------------------------------------------------------------------------
// Diagnostics thresholds
// ---------------------------------------------------------------------------
constexpr float SOILING_RATIO_WARN     = 0.80f;  // Rule A and Rule B both use 80 %
constexpr float SENSOR_MISMATCH_A      = 0.10f;  // INA219 vs ACS712: larger of 0.1 A ...
constexpr float SENSOR_MISMATCH_FRAC   = 0.20f;  // ... or 20 %
constexpr uint32_t MISMATCH_HOLD_MS    = DEMO_MODE ? 10000 : 60000;
constexpr float NO_CURRENT_A           = 0.010f; // below this with good light = no current
constexpr float PANEL_PRESENT_V        = 1.0f;   // INA219 bus voltage below this = panel disconnected
constexpr float LDR_MISMATCH_FRAC      = 0.15f;  // one pair below 15 % of the other ...
constexpr uint32_t LDR_MISMATCH_HOLD_MS = DEMO_MODE ? 5000 : 10000;
constexpr int   TILT_STALL_DEG         = 12;     // after the tilt moves this far towards the light ...
constexpr float TILT_STALL_SHRINK      = 0.70f;  // ... the imbalance must drop below 70 %, else TILT_NO_RESPONSE
constexpr float PANEL_OVERHEAT_C       = 60.0f;
constexpr float PANEL_HOT_VS_AIR_C     = 20.0f;
constexpr float HUMIDITY_HIGH          = 90.0f;
constexpr float SUPPLY_LOW_V           = 11.0f;
constexpr float SUPPLY_HIGH_V          = 14.0f;
constexpr uint32_t FAULT_HOLD_MS       = 3000;   // most faults must persist this long

// ---------------------------------------------------------------------------
// Wi-Fi: put your network in secrets.h (copy secrets.example.h).
// Without it, the ESP32 starts its own hotspot.
// ---------------------------------------------------------------------------
#define AP_SSID     "SolarTracker-Setup"
#define AP_PASSWORD "solar1234"
#define MDNS_NAME   "solar-tracker"
