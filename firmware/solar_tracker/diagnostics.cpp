#include "diagnostics.h"
#include "config.h"
#include "app_state.h"
#include "sensors.h"
#include "relays.h"
#include "tracker.h"
#include "cleaner.h"
#include <Preferences.h>

namespace {
  Preferences prefs;
  int lowCount = 0;
  bool alert = false;
  bool tempsSeen = false;

  // ---- fault hold timers -------------------------------------------------
  struct Rule { const char *code; const char *msg; uint32_t since; };
  Rule rules[] = {
    {"INA219_MISSING",          "INA219 not answering on I2C: ACS712 now measures current", 0},
    {"ACS712_FAULT",            "ACS712 output out of range: check its 5 V and divider", 0},
    {"CURRENT_SENSOR_MISMATCH", "INA219 and ACS712 disagree: one of them is faulty", 0},
    {"PANEL_DISCONNECTED",      "Light present but no panel voltage: check the panel wires", 0},
    {"NO_CURRENT",              "Panel voltage present but no current: check the load and relay 1", 0},
    {"LDR_MISMATCH",            "One LDR pair reads far lower than the other: covered or disconnected", 0},
    {"TILT_NO_RESPONSE",        "Tilt moved but the light balance did not change: check the tilt servo", 0},
    {"PANEL_OVERHEAT",          "Panel above 60 C", 0},
    {"PANEL_HOT_VS_AIR",        "Panel much hotter than the air: poor airflow or a hot spot", 0},
    {"TEMP_SENSOR",             "DS18B20 not responding (-127): check GPIO 4 and the pull-up", 0},
    {"DHT_SENSOR",              "DHT11 not responding: check GPIO 23", 0},
    {"HIGH_HUMIDITY",           "Humidity above 90 %: dew likely", 0},
    {"SUPPLY_LOW",              "12 V supply below 11 V", 0},
    {"SUPPLY_HIGH",             "12 V supply above 14 V", 0},
    {"SOILING",                 "Panel output low for this light: dust likely, clean the panel", 0},
    {"SELF_TEST_FAIL",          "Last self-test failed", 0},
    {"LIGHT_SATURATED",         "Light reference saturated: dust check paused", 0},
    {"NOT_CALIBRATED",          "No clean-panel baseline yet: clean the panel and press Calibrate", 0},
  };
  constexpr int NRULES = sizeof(rules) / sizeof(rules[0]);
  bool selfTestFailed = false;

  // Returns true once cond has been true for holdMs.
  bool hold(int i, bool cond, uint32_t holdMs) {
    if (!cond) { rules[i].since = 0; return false; }
    if (rules[i].since == 0) rules[i].since = millis() | 1;
    return millis() - rules[i].since >= holdMs;
  }

  bool lightGood() { return !isnan(app.s.photoMv) && app.s.photoMv > 2 * PHOTO_MIN_MV; }

  bool mismatch(float a, float b) {
    if (isnan(a) || isnan(b)) return false;
    float tol = max(SENSOR_MISMATCH_A, SENSOR_MISMATCH_FRAC * max(fabsf(a), fabsf(b)));
    return fabsf(a - b) > tol;
  }

  // ---- self-test -----------------------------------------------------------
  enum class Phase : uint8_t { Idle, Open, Loaded };
  Phase phase = Phase::Idle;
  uint32_t phaseStart = 0;
  String openNote;
}

namespace Diagnostics {

void begin() {
  prefs.begin("solar", false);
  app.soil.k = prefs.getFloat("k", NAN);
  app.soil.baseA = prefs.getFloat("baseA", NAN);
  app.soil.calibrated = !isnan(app.soil.k) && !isnan(app.soil.baseA);
}

void zeroAcsAtBoot() {
  Relays::setPanelConnected(false);
  delay(1500);
  Sensors::setAcsZero(Sensors::acsRawMv());
  Relays::setPanelConnected(true);
}

void afterPowerRead() {
  SensorData &s = app.s;
  Soiling &so = app.soil;
  // Which current sensor to trust: arbitration by the light reference.
  static uint32_t mismatchSince = 0;
  bool mm = s.inaOk && mismatch(s.inaA, s.acsA) && Relays::panelConnected();
  if (mm) {
    if (mismatchSince == 0) mismatchSince = millis() | 1;
    if (millis() - mismatchSince >= MISMATCH_HOLD_MS && so.calibrated && lightGood()) {
      float predicted = so.k * s.photoMv;
      app.trusted = fabsf(s.inaA - predicted) <= fabsf(s.acsA - predicted) ? Sensor::INA219 : Sensor::ACS712;
    }
  } else {
    mismatchSince = 0;
    if (s.inaOk) app.trusted = Sensor::INA219;
  }
  s.currentA = (app.trusted == Sensor::INA219 && s.inaOk) ? s.inaA : s.acsA;

  // Soiling ratios (logged every second; Rule B is the one that raises SOILING).
  // Rule A needs no light sensor, so it is judged whenever the panels are on the load;
  // Rule B (and soil_valid in the CSV) also needs the light reference in range.
  bool base = so.calibrated && Relays::panelConnected() && !Cleaner::busy() && phase == Phase::Idle &&
              !isnan(s.currentA);
  so.valid = base && s.photoMv > PHOTO_MIN_MV && s.photoMv < PHOTO_SATURATED_MV;
  if (so.calibrated && !isnan(s.currentA)) {
    so.ratioNaive = s.currentA / so.baseA;
    so.ratioLight = s.photoMv > PHOTO_MIN_MV ? s.currentA / (so.k * s.photoMv) : NAN;
  } else {
    so.ratioNaive = so.ratioLight = NAN;
  }
  so.ruleA = base && so.ratioNaive < SOILING_RATIO_WARN;
  so.ruleB = so.valid && so.ratioLight < SOILING_RATIO_WARN;
}

void soilingCheck() {
  if (!app.soil.valid) return;                 // keep the count; just skip this check
  lowCount = app.soil.ruleB ? lowCount + 1 : 0;
  alert = lowCount >= SOILING_CONFIRM;
}

bool soilingAlert() { return alert; }

bool calibrate(String &msg) {
  if (!Relays::panelConnected() || Cleaner::busy() || phase != Phase::Idle) { msg = "Busy: try again in a moment"; return false; }
  float sumI = 0, sumL = 0;
  const int N = 10;
  for (int i = 0; i < N; i++) {
    Sensors::readPower();
    sumI += app.s.currentA;
    sumL += app.s.photoMv;
    delay(300);
  }
  float I = sumI / N, L = sumL / N;
  if (L < 2 * PHOTO_MIN_MV)    { msg = "Too dark to calibrate: use steady bright light"; return false; }
  if (L > PHOTO_SATURATED_MV)  { msg = "Light reference saturated: use a 1 k resistor or less light"; return false; }
  if (I < NO_CURRENT_A)        { msg = "No panel current: check the load and relay 1"; return false; }
  app.soil.k = I / L;
  app.soil.baseA = I;
  app.soil.calibrated = true;
  prefs.putFloat("k", app.soil.k);
  prefs.putFloat("baseA", I);
  lowCount = 0;
  alert = false;
  msg = "Calibrated: baseline " + String(I * 1000, 1) + " mA at " + String(L, 0) + " mV light";
  return true;
}

bool startSelfTest(String &msg) {
  if (phase != Phase::Idle) { msg = "Self-test already running"; return false; }
  if (Cleaner::busy())      { msg = "Wiper running: try after cleaning"; return false; }
  Relays::setPanelConnected(false);
  phase = Phase::Open;
  phaseStart = millis();
  app.selfTest = "running";
  msg = "Self-test started (about 5 s)";
  return true;
}

bool selfTestActive() { return phase != Phase::Idle; }

void updateSelfTest() {
  if (phase == Phase::Idle || millis() - phaseStart < 2500) return;
  Sensors::readPower();
  SensorData &s = app.s;
  if (phase == Phase::Open) {
    // Step 1: panels disconnected. Both sensors must read about zero.
    bool inaZero = !s.inaOk || fabsf(s.inaA) < 0.03f;
    float acsMv = Sensors::acsRawMv();
    bool acsZero = fabsf(acsMv - Sensors::acsZero()) < 0.05f * ACS712_MV_PER_A * 4;  // within ~0.2 A of old zero
    String fail;
    if (!inaZero) fail = "INA219 read " + String(s.inaA * 1000, 0) + " mA with the panels disconnected";
    else if (!acsZero) fail = "ACS712 zero drifted far: check its 5 V supply";
    Sensors::setAcsZero(acsMv);                      // re-zero for the next readings
    openNote = isnan(s.panelV) ? "Voc n/a" : "Voc " + String(s.panelV, 2) + " V";
    Relays::setPanelConnected(true);
    if (fail.length()) { app.selfTest = "FAIL: " + fail; selfTestFailed = true; phase = Phase::Idle; return; }
    phase = Phase::Loaded;
    phaseStart = millis();
    return;
  }
  // Step 2: panels on the known load. Sensors agree and V = I x R.
  String result;
  bool ok = true;
  if (s.currentA < 0.02f) {
    result = "PASS (zero check only, too dark for the load check), " + openNote;
  } else if (s.inaOk && mismatch(s.inaA, s.acsA)) {
    ok = false; result = "FAIL: INA219 " + String(s.inaA * 1000, 0) + " mA vs ACS712 " + String(s.acsA * 1000, 0) + " mA";
  } else if (s.inaOk && fabsf(s.panelV - s.inaA * TEST_LOAD_OHMS) > 0.25f * s.inaA * TEST_LOAD_OHMS + 0.1f) {
    ok = false; result = "FAIL: V " + String(s.panelV, 2) + " V does not match I x R = " + String(s.inaA * TEST_LOAD_OHMS, 2) + " V";
  } else {
    result = "PASS: " + openNote + ", load " + String(s.currentA * 1000, 0) + " mA";
  }
  app.selfTest = result;
  selfTestFailed = !ok;
  phase = Phase::Idle;
}

void evaluateFaults() {
  const SensorData &s = app.s;
  const TrackStatus &t = Tracker::status();
  bool testing = phase != Phase::Idle;
  bool connected = Relays::panelConnected() && !testing;
  if (!isnan(s.airC) || (!isnan(s.panelC) && s.panelC > -100)) tempsSeen = true;

  bool on[NRULES];
  on[0]  = hold(0,  !s.inaOk, FAULT_HOLD_MS);
  on[1]  = hold(1,  !isnan(s.acsPinMv) && (s.acsPinMv < 150 || s.acsPinMv > 2400), FAULT_HOLD_MS);
  on[2]  = hold(2,  connected && s.inaOk && mismatch(s.inaA, s.acsA), MISMATCH_HOLD_MS);
  on[3]  = hold(3,  connected && s.inaOk && s.panelV < PANEL_PRESENT_V && lightGood(), FAULT_HOLD_MS);
  on[4]  = hold(4,  connected && s.inaOk && s.panelV > 2.0f && fabsf(s.currentA) < NO_CURRENT_A && lightGood(), FAULT_HOLD_MS);
  int hi = max(s.ldrA, s.ldrB), lo = min(s.ldrA, s.ldrB);
  on[5]  = hold(5,  !t.dark && hi > LDR_DARK_LEVEL && lo < LDR_MISMATCH_FRAC * hi, LDR_MISMATCH_HOLD_MS);
  on[6]  = hold(6,  t.stalled, 0);
  on[7]  = hold(7,  s.panelC > PANEL_OVERHEAT_C, 0);
  on[8]  = hold(8,  s.panelC > -100 && !isnan(s.airC) && s.panelC - s.airC > PANEL_HOT_VS_AIR_C, 0);
  on[9]  = hold(9,  tempsSeen && (isnan(s.panelC) || s.panelC <= -100), 0);
  on[10] = hold(10, tempsSeen && isnan(s.airC), 0);
  on[11] = hold(11, s.humidity > HUMIDITY_HIGH, 0);
  on[12] = hold(12, !isnan(s.supplyV) && s.supplyV < SUPPLY_LOW_V, FAULT_HOLD_MS);
  on[13] = hold(13, s.supplyV > SUPPLY_HIGH_V, FAULT_HOLD_MS);
  on[14] = alert;
  on[15] = selfTestFailed;
  on[16] = s.photoMv >= PHOTO_SATURATED_MV;
  on[17] = !app.soil.calibrated;

  String out;
  for (int i = 0; i < NRULES; i++) {
    if (!on[i]) continue;
    if (out.length()) out += '|';
    out += rules[i].code;
  }
  app.faults = out;
}

const char *describe(const String &code) {
  for (int i = 0; i < NRULES; i++)
    if (code == rules[i].code) return rules[i].msg;
  return "";
}

}  // namespace Diagnostics
