// Solar Tracker — ESP32 firmware v0.6 (as-built demo rig, 12 V adapter)
//
// Single-axis tracking (tilt servo + 2 LDR pairs), servo wiper, dual current
// sensing (INA219 + ACS712) with relay self-test, light-referenced dust check
// (photo transistor), DS18B20 + DHT11 temperatures, Wi-Fi dashboard, ThingSpeak
// upload of every reading with phone commands and remote tests (TalkBack), and a
// 1-second CSV log on Serial for the experiments in experiments/PROTOCOL.md.
//
// Board: ESP32 Dev Module (WROOM-32). Serial Monitor: 115200 baud, Newline.

#include "config.h"
#include "app_state.h"
#include "relays.h"
#include "sensors.h"
#include "tracker.h"
#include "cleaner.h"
#include "diagnostics.h"
#include "commands.h"
#include "logger.h"
#include "web.h"
#include "cloud.h"
#include "tests.h"

namespace {
  uint32_t tLog = 0, tTrack = 0, tTemp = 0, tSupply = 0, tSoil = 0, tSelfTest = 0;
  uint32_t lastAutoClean = 0;
  String serialLine;

  bool due(uint32_t &last, uint32_t interval) {
    if (millis() - last < interval) return false;
    last = millis();
    return true;
  }

  void readSerial() {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        if (serialLine.length()) { Serial.print("# "); Serial.println(Commands::run(serialLine)); }
        serialLine = "";
      } else if (serialLine.length() < 80) {
        serialLine += c;
      }
    }
  }

  // Servo power (relay 2): always on in demo mode; otherwise only while moving.
  void manageServoPower() {
    bool need = DEMO_MODE || Cleaner::busy() || Tests::busy() || Tracker::overridden() ||
                millis() - Tracker::status().lastMoveMs < SERVO_HOLD_MS ||
                (Cleaner::lastTestMs() && millis() - Cleaner::lastTestMs() < 3000);
    if (need != Relays::servoPower()) Relays::setServoPower(need);
  }
}

void setup() {
  Relays::begin();                 // first: both relays off, no clicks at boot
  Serial.begin(115200);
  delay(300);
  Serial.println(F("# Solar Tracker v0.6"));
  Sensors::begin();
  Tracker::begin();
  Cleaner::begin();
  Diagnostics::begin();
  Diagnostics::zeroAcsAtBoot();    // panels disconnected for 1.5 s to zero the ACS712
  if (DEMO_MODE) Relays::setServoPower(true);
  Sensors::readTemps();
  Sensors::readSupply();
  Sensors::readPower();
  Serial.printf("# INA219 %s, ACS712 zero %.0f mV, photo %.0f mV, supply %.2f V, calibrated %s\n",
                app.s.inaOk ? "OK" : "MISSING", Sensors::acsZero(), app.s.photoMv, app.s.supplyV,
                app.soil.calibrated ? "yes" : "no");
  Web::begin();
  Cloud::begin();
  Commands::printHelp();
  Logger::printHeader();
}

void loop() {
  readSerial();
  Cloud::handleCommands();
  Web::handle();
  Cleaner::update();
  Diagnostics::updateSelfTest();
  Tests::update();
  Tracker::service();              // moves the tilt servo one degree at a time
  Commands::service();

  if (due(tTrack, TRACK_INTERVAL_MS))
    Tracker::update(Cleaner::busy() || Diagnostics::selfTestActive() || Tests::busy());
  if (due(tTemp, TEMP_INTERVAL_MS))     Sensors::readTemps();
  if (due(tSupply, SUPPLY_INTERVAL_MS)) Sensors::readSupply();

  if (due(tLog, LOG_INTERVAL_MS)) {
    if (!Diagnostics::selfTestActive()) Sensors::readPower();
    Diagnostics::afterPowerRead();
    Diagnostics::evaluateFaults();
    Logger::printRow();
    Cloud::snapshot();
  }

  if (due(tSoil, SOILING_INTERVAL_MS)) {
    Diagnostics::soilingCheck();
    bool gapOk = lastAutoClean == 0 || millis() - lastAutoClean > AUTO_CLEAN_GAP_MS;
    if (app.autoClean && Diagnostics::soilingAlert() && gapOk && !Cleaner::busy() && !Tests::busy()) {
      lastAutoClean = millis();
      Cleaner::start();
      app.mark = "autoclean";
      Serial.println(F("# Auto-clean: dust confirmed, cleaning"));
    }
  }

  if (due(tSelfTest, SELFTEST_EVERY_MS) && !Tests::busy()) {
    String msg;
    Diagnostics::startSelfTest(msg);
  }

  manageServoPower();
}
