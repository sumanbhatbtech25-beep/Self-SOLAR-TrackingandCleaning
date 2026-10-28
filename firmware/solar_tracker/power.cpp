#include "power.h"
#include "config.h"
#include <esp_sleep.h>

// RTC memory survives deep sleep (but not a power cut or reset button).
RTC_DATA_ATTR static uint8_t rtcSleepReason = 0;
RTC_DATA_ATTR static bool    rtcLowLatched  = false;
RTC_DATA_ATTR static bool    rtcTiltValid   = false;
RTC_DATA_ATTR static int     rtcTilt        = 0;

namespace {
  SleepReason bootReason = SleepReason::None;

  int lightLevel() {
    uint32_t sum = 0;
    for (int i = 0; i < LDR_SAMPLES; i++) sum += analogRead(PIN_LDR_EAST) + analogRead(PIN_LDR_WEST);
    return static_cast<int>(sum / (2 * LDR_SAMPLES));
  }
}

namespace Power {

float readBatteryVoltage() {
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(PIN_BATTERY);   // factory-calibrated
  return (mv / 16.0f) / 1000.0f * BATTERY_DIVIDER * BATTERY_CAL;
}

void deepSleep(uint32_t minutes, SleepReason why, int tiltToRestore) {
  rtcSleepReason = static_cast<uint8_t>(why);
  if (why == SleepReason::LowBattery) rtcLowLatched = true;
  rtcTilt = tiltToRestore;
  rtcTiltValid = true;
  Serial.printf("Deep sleep for %lu min (%s)\n", (unsigned long)minutes,
                why == SleepReason::Night ? "night" : "low battery");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(uint64_t(minutes) * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
  while (true) {}   // never reached
}

void beginEarly() {
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);
  analogSetPinAttenuation(PIN_LDR_EAST, ADC_11db);
  analogSetPinAttenuation(PIN_LDR_WEST, ADC_11db);

  bool timerWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
  bootReason = timerWake ? static_cast<SleepReason>(rtcSleepReason) : SleepReason::None;
  if (!timerWake) { rtcLowLatched = false; rtcTiltValid = false; }
  rtcSleepReason = 0;

  if (BATTERY_ENABLED) {
    float v = readBatteryVoltage();
    if (v >= BATTERY_MISSING_V) {
      // Hysteresis: once latched low, stay asleep until the battery recovers.
      if (rtcLowLatched && v < BATTERY_RESUME_V)
        deepSleep(LOW_BATTERY_SLEEP_MIN, SleepReason::LowBattery, rtcTilt);
      if (!rtcLowLatched && v < BATTERY_CUTOFF_V) {
        rtcLowLatched = true;
        deepSleep(LOW_BATTERY_SLEEP_MIN, SleepReason::LowBattery, rtcTiltValid ? rtcTilt : 0);
      }
      rtcLowLatched = false;
    }
  }

  // Woke from a night nap: if it's still dark, go straight back to sleep
  // without starting Wi-Fi or moving the servos.
  if (NIGHT_DEEP_SLEEP && bootReason == SleepReason::Night && lightLevel() < DARK_THRESHOLD)
    deepSleep(NIGHT_SLEEP_MIN, SleepReason::Night, rtcTilt);
}

void update(BatteryData &b) {
  if (!BATTERY_ENABLED) { b = BatteryData(); return; }
  float v = readBatteryVoltage();
  b.voltage  = v;
  b.present  = v >= BATTERY_MISSING_V;
  if (!b.present) { b.percent = -1; b.full = b.low = b.critical = false; return; }
  float pct = (v - BATTERY_EMPTY_REST_V) / (BATTERY_FULL_REST_V - BATTERY_EMPTY_REST_V) * 100.0f;
  b.percent  = constrain(int(pct + 0.5f), 0, 100);   // rough: reads high while charging
  b.full     = v >= BATTERY_FULL_V;
  b.low      = v < BATTERY_LOW_V;
  b.critical = v < BATTERY_CUTOFF_V;
}

SleepReason lastSleepReason() { return bootReason; }
bool hasSavedTilt() { return rtcTiltValid; }
int  savedTilt()    { return rtcTilt; }

}  // namespace Power
