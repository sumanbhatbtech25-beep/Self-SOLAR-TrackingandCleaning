// config.h — every pin, threshold and timing value in one place.
// Change values here, not inside the modules.
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pins (ESP32 DevKit V1, 30-pin). See docs/wiring.md.
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_DS18B20     = 4;   // DS18B20 data, 4.7 kΩ pull-up to 3V3
constexpr uint8_t PIN_SERVO_TILT  = 18;  // tilt servo signal
constexpr uint8_t PIN_SERVO_WIPER = 19;  // wiper servo signal
constexpr uint8_t PIN_I2C_SDA     = 21;  // INA219 SDA
constexpr uint8_t PIN_I2C_SCL     = 22;  // INA219 SCL
constexpr uint8_t PIN_LDR_EAST    = 34;  // ADC1 only — ADC2 stops working with Wi-Fi on
constexpr uint8_t PIN_LDR_WEST    = 35;  // ADC1 only
constexpr uint8_t PIN_BATTERY     = 32;  // ADC1, battery divider 100 kΩ / 27 kΩ

constexpr uint8_t INA219_ADDRESS  = 0x40;

// ---------------------------------------------------------------------------
// Tilt servo and tracking
// ---------------------------------------------------------------------------
// Servo angle (0–180) at which the panel lies flat. Set this during bring-up.
constexpr int   TILT_SERVO_CENTER  = 90;
// Allowed travel either side of flat, in degrees. Negative tilt = facing east.
constexpr int   TILT_LIMIT_DEG     = 60;
// Set true if the panel turns AWAY from the brighter LDR (servo mounted mirrored).
constexpr bool  TILT_INVERT        = false;
// Degrees moved per tracking step.
constexpr int   TILT_STEP_DEG      = 1;
// Tracking step interval. 1° per 100 ms = 10°/s maximum slew.
constexpr uint32_t TRACK_INTERVAL_MS = 100;
// Relative LDR imbalance (E−W)/(E+W) ignored as noise.
constexpr float TRACK_DEADBAND     = 0.04f;
// Average LDR reading (0–4095) below which it is treated as night.
constexpr int   DARK_THRESHOLD     = 150;
// At night the panel walks back to face sunrise, one step every N tracking steps.
constexpr int   NIGHT_PARK_DIVIDER = 5;

// Servo pulse range in microseconds (SG90 / MG90S).
constexpr int SERVO_MIN_US = 500;
constexpr int SERVO_MAX_US = 2400;

// ---------------------------------------------------------------------------
// Wiper
// ---------------------------------------------------------------------------
constexpr int      WIPER_PARK_DEG   = 0;    // blade resting off the glass
constexpr int      WIPER_END_DEG    = 170;  // far end of the sweep
constexpr int      WIPER_PASSES     = 2;    // full out-and-back sweeps per cycle
constexpr int      WIPER_STEP_DEG   = 2;
constexpr uint32_t WIPER_STEP_MS    = 12;   // ≈ 1 s per one-way sweep
// Minimum time between automatic cleaning cycles.
constexpr uint32_t AUTO_CLEAN_COOLDOWN_MS = 30UL * 60UL * 1000UL;  // 30 min
// Soiling must be detected continuously this long before auto-clean fires.
constexpr uint32_t AUTO_CLEAN_CONFIRM_MS  = 60UL * 1000UL;         // 1 min

// ---------------------------------------------------------------------------
// Battery (12 V sealed lead-acid behind a PWM solar charge controller)
// ---------------------------------------------------------------------------
// Set false for bench tests powered from a 5 V adapter with no battery fitted.
constexpr bool  BATTERY_ENABLED      = true;
// Divider: BAT+ — 100 kΩ — GPIO 32 — 27 kΩ — GND. 14.4 V in -> 3.06 V at the pin.
constexpr float BATTERY_DIVIDER      = (100.0f + 27.0f) / 27.0f;
// Fine-tune so the dashboard matches a multimeter across the battery terminals.
constexpr float BATTERY_CAL          = 1.00f;
constexpr float BATTERY_MISSING_V    = 5.0f;   // below this = divider not connected
constexpr float BATTERY_FULL_V       = 13.6f;  // controller is limiting charge at/above this
constexpr float BATTERY_HIGH_V       = 15.0f;  // overcharge: controller fault
constexpr float BATTERY_LOW_V        = 12.0f;  // warning
constexpr float BATTERY_CUTOFF_V     = 11.8f;  // enter low-battery sleep
constexpr float BATTERY_RESUME_V     = 12.3f;  // wake up properly only above this
constexpr float BATTERY_EMPTY_REST_V = 11.8f;  // 0 % for the rough charge estimate
constexpr float BATTERY_FULL_REST_V  = 12.8f;  // 100 % for the rough charge estimate
constexpr uint32_t BATTERY_CUTOFF_CONFIRM_MS = 30000;  // must stay low this long
constexpr uint32_t LOW_BATTERY_SLEEP_MIN     = 10;     // check again every 10 min

// Night deep sleep: saves ~6–8 Wh per night. The dashboard is offline while asleep.
constexpr bool     NIGHT_DEEP_SLEEP     = true;
constexpr uint32_t NIGHT_SLEEP_MIN      = 15;              // wake, check light, sleep again
constexpr uint32_t NIGHT_CONFIRM_MS     = 5UL * 60UL * 1000UL;  // dark and parked this long first

// ---------------------------------------------------------------------------
// Panel and measurement
// ---------------------------------------------------------------------------
constexpr float PANEL_RATED_W      = 10.0f;  // nameplate power
constexpr float PANEL_VOC          = 21.6f;  // open-circuit voltage from the label
constexpr uint32_t SENSOR_INTERVAL_MS = 1000;
constexpr int   LDR_SAMPLES        = 8;       // readings averaged per LDR sample

// ---------------------------------------------------------------------------
// Diagnostic thresholds (see docs/diagnostics.md)
// ---------------------------------------------------------------------------
constexpr float DIAG_OPEN_V          = 19.5f;  // V above this with no current = open circuit
constexpr float DIAG_MIN_CURRENT_A   = 0.010f; // 10 mA
constexpr float DIAG_NO_VOLTAGE_V    = 1.0f;   // bright light but under 1 V = panel disconnected
constexpr float DIAG_OVERHEAT_C      = 60.0f;
constexpr float DIAG_SOILING_RATIO   = 0.80f;  // current below 80 % of the clean baseline
constexpr int   DIAG_BRIGHT_LDR      = 1500;   // average LDR above this = strong light
constexpr int   DIAG_LDR_SATURATED   = 4000;   // above this the LDR can't measure brightness
constexpr float DIAG_LDR_MISMATCH    = 0.15f;  // one LDR below 15 % of the other
constexpr uint32_t DIAG_LDR_MISMATCH_MS = 10000;
constexpr uint32_t DIAG_STALL_MS     = 4000;   // imbalance not shrinking while moving

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------
constexpr char     MDNS_HOSTNAME[]    = "solar-tracker";  // http://solar-tracker.local
constexpr char     AP_FALLBACK_SSID[] = "SolarTracker-Setup";
constexpr char     AP_FALLBACK_PASS[] = "solar1234";      // 8+ chars required
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

// Print one CSV line per sensor read on Serial (for logging / future TinyML data).
constexpr bool SERIAL_CSV_LOG = true;
