/*
 * Solar Tracker ESP32 — single-board prototype
 *
 *  - Single-axis sun tracking from two LDRs (rule-based)
 *  - Servo wiper cleaning, manual or automatic on detected soiling
 *  - Panel power monitoring (INA219) and temperature (DS18B20)
 *  - 12 V battery storage: voltage monitoring, low-battery protection,
 *    deep sleep at night
 *  - Rule-based fault diagnostics
 *  - Wi-Fi dashboard and JSON API
 *
 * Wiring: docs/wiring.md        Settings: config.h
 * Wi-Fi:  copy secrets.example.h to secrets.h. Without it the board starts
 *         its own access point "SolarTracker-Setup" (password in config.h).
 */
#include <WiFi.h>
#include <ESPmDNS.h>
#include "config.h"
#include "sensors.h"
#include "tracker.h"
#include "cleaner.h"
#include "diagnostics.h"
#include "power.h"
#include "app_state.h"
#include "web.h"

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #define WIFI_SSID     ""
  #define WIFI_PASSWORD ""
#endif

static uint32_t lastSensorRead    = 0;
static uint32_t soilingSince      = 0;
static uint32_t lastAutoClean     = 0;
static bool     wasCleaning       = false;
static uint32_t recalibrateAt     = 0;
static Severity lastWorst         = Severity::Ok;
static String   lastFindingCodes;
static uint32_t batteryLowSince   = 0;
static uint32_t nightParkedSince  = 0;

static void startWifi() {
  WiFi.setHostname(MDNS_HOSTNAME);
  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("Connecting to %s", WIFI_SSID);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    app.stationMode = true;
    app.ip = WiFi.localIP();
    app.log.add("Wi-Fi connected, %s", app.ip.toString().c_str());
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_FALLBACK_SSID, AP_FALLBACK_PASS);
    app.stationMode = false;
    app.ip = WiFi.softAPIP();
    app.log.add("Access point %s at %s", AP_FALLBACK_SSID, app.ip.toString().c_str());
  }

  if (MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http", "tcp", 80);
  Serial.printf("Dashboard: http://%s/  or  http://%s.local/\n", app.ip.toString().c_str(), MDNS_HOSTNAME);
}

// Log a line whenever the set of warning/critical findings changes.
static void logFindingChanges() {
  String codes;
  for (int i = 0; i < app.diag.count; i++) {
    const Finding &f = app.diag.items[i];
    if (f.sev >= Severity::Warn) { codes += f.code; codes += ' '; }
  }
  if (codes != lastFindingCodes) {
    for (int i = 0; i < app.diag.count; i++) {
      const Finding &f = app.diag.items[i];
      if (f.sev >= Severity::Warn && lastFindingCodes.indexOf(f.code) < 0)
        app.log.add("%s: %s", Diagnostics::severityName(f.sev), f.code);
    }
    if (codes.length() == 0 && lastWorst >= Severity::Warn) app.log.add("All faults cleared");
    lastFindingCodes = codes;
  }
  lastWorst = app.diag.worst;
}

static void handleAutoClean(uint32_t now) {
  if (!app.autoClean || !app.diag.soiling) { soilingSince = 0; return; }
  if (soilingSince == 0) soilingSince = now;
  bool confirmed = now - soilingSince >= AUTO_CLEAN_CONFIRM_MS;
  bool cooledDown = lastAutoClean == 0 || now - lastAutoClean >= AUTO_CLEAN_COOLDOWN_MS;
  if (confirmed && cooledDown && !Cleaner::busy()) {
    if (Cleaner::start("auto")) {
      app.log.add("Auto-clean: output %.0f%% of expected", app.diag.soilingRatio * 100);
      lastAutoClean = now;
      soilingSince = 0;
    }
  }
}

// After every cleaning cycle, wait for the tracker to settle and re-record the clean baseline.
static void handleRecalibration(uint32_t now) {
  bool cleaning = Cleaner::busy();
  if (wasCleaning && !cleaning) {
    recalibrateAt = now + 10000;
    app.log.add("Cleaning cycle finished");
  }
  wasCleaning = cleaning;

  if (recalibrateAt && now >= recalibrateAt) {
    recalibrateAt = 0;
    const char *reason = "";
    if (Diagnostics::calibrate(app.sensors, Tracker::status(), app.battery, &reason))
      app.log.add("Clean baseline updated");
    else
      app.log.add("Baseline not updated: %s", reason);
  }
}

// Protect the battery: sustained voltage under the cut-off puts the board to sleep.
static void handleLowBattery(uint32_t now) {
  if (!BATTERY_ENABLED || !app.battery.present || !app.battery.critical) { batteryLowSince = 0; return; }
  if (batteryLowSince == 0) batteryLowSince = now;
  if (now - batteryLowSince >= BATTERY_CUTOFF_CONFIRM_MS && !Cleaner::busy()) {
    app.log.add("Battery %.2f V: low-battery sleep", app.battery.voltage);
    Power::deepSleep(LOW_BATTERY_SLEEP_MIN, SleepReason::LowBattery, Tracker::status().tiltDeg);
  }
}

// Once it has been dark and the panel has parked facing sunrise for a while, nap.
static void handleNightSleep(uint32_t now) {
  const TrackerStatus &t = Tracker::status();
  bool parked = app.diag.night && t.state == TrackerState::NightPark && t.tiltDeg <= -TILT_LIMIT_DEG;
  if (!NIGHT_DEEP_SLEEP || !parked || Cleaner::busy()) { nightParkedSince = 0; return; }
  if (nightParkedSince == 0) nightParkedSince = now;
  if (now - nightParkedSince >= NIGHT_CONFIRM_MS) {
    app.log.add("Night: sleeping in %lu-minute naps", (unsigned long)NIGHT_SLEEP_MIN);
    Power::deepSleep(NIGHT_SLEEP_MIN, SleepReason::Night, t.tiltDeg);
  }
}

static void printCsv() {
  const SensorData &s = app.sensors;
  const TrackerStatus &t = Tracker::status();
  Serial.printf("%lu,%.3f,%.4f,%.3f,%.2f,%.2f,%d,%d,%d,%s,%.3f,%d\n",
                (unsigned long)s.timestamp, s.busVoltage, s.current, s.power,
                s.tempOk ? s.panelTempC : -127.0f,
                isnan(app.battery.voltage) ? 0.0f : app.battery.voltage,
                s.ldrEast, s.ldrWest, t.tiltDeg,
                Tracker::stateName(t.state),
                isnan(app.diag.soilingRatio) ? -1.0f : app.diag.soilingRatio,
                Cleaner::busy() ? 1 : 0);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nSolar Tracker ESP32 starting");

  // First: may send the board straight back to sleep (low battery, or still night).
  Power::beginEarly();

  if (!Sensors::begin()) app.log.add("INA219 not found on I2C");
  Diagnostics::begin();
  Tracker::begin(Power::hasSavedTilt() ? Power::savedTilt() : 0);
  Cleaner::begin();
  startWifi();
  Web::begin();

  if (Power::lastSleepReason() == SleepReason::Night)      app.log.add("Morning: woke from night sleep");
  if (Power::lastSleepReason() == SleepReason::LowBattery) app.log.add("Battery recovered, resuming");
  if (Diagnostics::baseline() > 0) app.log.add("Loaded clean baseline from flash");
  if (SERIAL_CSV_LOG)
    Serial.println("ms,voltage_v,current_a,power_w,panel_temp_c,battery_v,ldr_east,ldr_west,tilt_deg,state,soiling_ratio,wiper");
}

void loop() {
  uint32_t now = millis();

  Web::handle();
  Cleaner::update();
  Tracker::setPaused(Cleaner::busy());   // hold position while the blade is on the glass
  Tracker::update();

  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    Sensors::update(app.sensors);
    Power::update(app.battery);
    Diagnostics::evaluate(app.sensors, Tracker::status(), app.battery, app.diag);
    logFindingChanges();
    handleAutoClean(now);
    handleLowBattery(now);
    handleNightSleep(now);
    if (SERIAL_CSV_LOG) printCsv();
  }

  handleRecalibration(now);
}
