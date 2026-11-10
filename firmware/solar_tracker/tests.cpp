#include "tests.h"
#include "config.h"
#include "app_state.h"
#include "sensors.h"
#include "tracker.h"
#include "cleaner.h"
#include "diagnostics.h"
#include "cloud.h"

namespace {
  enum class Step : uint8_t { Idle, SelfTest, TiltPos, TiltNeg, TiltHome, Wiper };
  Step step = Step::Idle;
  bool full = false;
  bool atEnd = false;           // tilt test: reached the current end, waiting to settle
  uint32_t endSince = 0;
  uint32_t wiperStart = 0;
  float imbPos = NAN, imbNeg = NAN;
  String rSensors, rSelf, rTilt, rWiper;
  bool wasSelfTest = false, wasCleaning = false;

  void add(String &list, const String &item) {
    if (list.length()) list += "; ";
    list += item;
  }

  // First word of a result ("PASS", "FAIL", "CHECK", ...), without a trailing colon.
  String verdict(const String &r) {
    int sp = r.indexOf(' ');
    String w = sp < 0 ? r : r.substring(0, sp);
    if (w.endsWith(":")) w.remove(w.length() - 1);
    return w;
  }

  float imbalanceNow() {
    float sum = 0;
    for (int i = 0; i < 5; i++) {
      Sensors::readLdrs();
      int a = app.s.ldrA, b = app.s.ldrB;
      sum += (a + b) > 0 ? float(a - b) / (a + b) : 0;
      delay(20);
    }
    return sum / 5;
  }

  String sensorTest() {
    Sensors::readPower();
    Sensors::readLdrs();
    Sensors::readSupply();
    Sensors::readTemps();
    const SensorData &s = app.s;
    String fails, checks;

    if (!s.inaOk) add(fails, "INA219 not found (I2C 0x40)");
    if (isnan(s.acsPinMv) || s.acsPinMv < 150 || s.acsPinMv > 2400)
      add(fails, "ACS712 pin " + String(s.acsPinMv, 0) + " mV (expect ~1250)");
    if (s.inaOk && s.photoMv > 2 * PHOTO_MIN_MV && s.panelV < PANEL_PRESENT_V)
      add(fails, "no panel voltage in light");
    if (isnan(s.panelC))            add(checks, "DS18B20 no reading yet");
    else if (s.panelC <= -100)      add(fails, "DS18B20 not found (-127)");
    if (isnan(s.airC) || isnan(s.humidity)) add(fails, "DHT11 no reply");
    if (isnan(s.supplyV) || s.supplyV < SUPPLY_LOW_V || s.supplyV > SUPPLY_HIGH_V)
      add(fails, "supply " + String(s.supplyV, 1) + " V");

    // Light sensors: 0 V can mean darkness or a loose wire, so these are CHECKs.
    if (s.photoMv < 20)                       add(checks, "photo 0 V (dark or unplugged)");
    else if (s.photoMv >= PHOTO_SATURATED_MV) add(checks, "photo saturated");
    int rawA = LDR_LIGHT_LOWERS_READING ? 4095 - s.ldrA : s.ldrA;
    int rawB = LDR_LIGHT_LOWERS_READING ? 4095 - s.ldrB : s.ldrB;
    if (rawA < 40) add(checks, "LDR A 0 V (unplugged?)");
    if (rawB < 40) add(checks, "LDR B 0 V (unplugged?)");

    if (fails.length()) return "FAIL: " + fails + (checks.length() ? " | CHECK: " + checks : String(""));
    if (checks.length()) return "CHECK: " + checks;
    return "PASS: panel " + String(s.panelV, 2) + " V " + String(s.currentA * 1000, 0) + " mA, light " +
           String(s.photoMv, 0) + " mV, LDR " + String(s.ldrA) + "/" + String(s.ldrB) + ", " +
           String(s.panelC, 1) + "/" + String(s.airC, 0) + " C, " + String(s.humidity, 0) + " %, " +
           String(s.supplyV, 1) + " V";
  }

  void report(const String &r) {
    app.testResult = r;
    Serial.print(F("# Test: "));
    Serial.println(r);
  }

  void startTilt() {
    imbPos = imbNeg = NAN;
    atEnd = false;
    Tracker::setOverride(true, TILT_RANGE_DEG);
    step = Step::TiltPos;
    if (!full) app.testResult = "running: tilt";
  }

  void finishAll();

  bool startWiper() {
    if (!Cleaner::start(1)) {
      rWiper = "FAIL: wiper busy";
      if (full) finishAll(); else { step = Step::Idle; report(rWiper); }
      return false;
    }
    wiperStart = millis();
    step = Step::Wiper;
    if (!full) app.testResult = "running: wiper";
    return true;
  }

  void finishAll() {
    step = Step::Idle;
    app.mark = "test_end";
    if (full) {
      String parts[] = {rSensors, rSelf, rTilt, rWiper};
      String overall = "PASS";
      for (const String &p : parts) {
        String v = verdict(p);
        if (v == "FAIL") overall = "FAIL";
        else if (v != "PASS" && overall == "PASS") overall = "CHECK";
      }
      Serial.println("# Test sensors: " + rSensors);
      Serial.println("# Test current: " + rSelf);
      Serial.println("# Test tilt:    " + rTilt);
      Serial.println("# Test wiper:   " + rWiper);
      report("FULL " + overall + ": sensors " + verdict(rSensors) + ", current " + verdict(rSelf) +
             ", tilt " + verdict(rTilt) + ", wiper " + verdict(rWiper));
    }
    Cloud::requestUpload();
  }

  // Tilt sweep: +limit, -limit, flat. One call per loop.
  void updateTilt() {
    if (!Tracker::settled()) return;
    if (step == Step::TiltHome) {
      Tracker::setOverride(false);
      float change = imbPos - imbNeg;
      String nums = "balance " + String(imbPos, 2) + " at +" + String(TILT_RANGE_DEG) +
                    ", " + String(imbNeg, 2) + " at -" + String(TILT_RANGE_DEG);
      rTilt = fabsf(change) >= TILT_TEST_MIN_CHANGE
                ? "PASS: " + nums
                : "CHECK: " + nums + " (hardly changed: shine a torch from one side and retest)";
      if (full) { startWiper(); return; }
      report(rTilt);
      finishAll();
      return;
    }
    if (!atEnd) { atEnd = true; endSince = millis(); return; }
    if (millis() - endSince < TILT_TEST_SETTLE_MS) return;
    atEnd = false;
    if (step == Step::TiltPos) {
      imbPos = imbalanceNow();
      Tracker::setOverride(true, -TILT_RANGE_DEG);
      step = Step::TiltNeg;
    } else {
      imbNeg = imbalanceNow();
      Tracker::setOverride(true, 0);
      step = Step::TiltHome;
    }
  }
}

namespace Tests {

bool busy() { return step != Step::Idle; }

bool start(const String &which, String &msg) {
  if (busy())                        { msg = "A test is already running (send stop to end it)"; return false; }
  if (Diagnostics::selfTestActive()) { msg = "Self-test running: try again in 5 s"; return false; }
  bool needsServos = which == "tilt" || which == "wiper" || which == "full";
  if (needsServos && Cleaner::busy()) { msg = "Wiper running: try after cleaning"; return false; }

  app.mark = "test_" + which;
  full = false;
  if (which == "sensors") {
    rSensors = sensorTest();
    report(rSensors);
    Cloud::requestUpload();
    msg = "Sensor test: " + rSensors;
    return true;
  }
  if (which == "tilt")  { startTilt();  msg = "Tilt test started (about 15 s)"; return true; }
  if (which == "wiper") {
    bool ok = startWiper();
    msg = ok ? "Wiper test started (one pass)" : rWiper;
    return ok;
  }
  if (which == "full") {
    full = true;
    rSensors = sensorTest();
    rSelf = rTilt = rWiper = "not run";
    app.testResult = "running: full test";
    String m;
    if (Diagnostics::startSelfTest(m)) step = Step::SelfTest;
    else { rSelf = "FAIL: " + m; startTilt(); }
    msg = "Full test started (about 40 s): sensors, current sensors, tilt, wiper";
    return true;
  }
  msg = "Use: test sensors | tilt | wiper | full";
  app.mark = "";
  return false;
}

void stop() {
  if (!busy()) return;
  if (step == Step::TiltPos || step == Step::TiltNeg || step == Step::TiltHome) Tracker::setOverride(false);
  if (step == Step::Wiper) Cleaner::stop();
  step = Step::Idle;
  report("stopped");
  Cloud::requestUpload();
}

void update() {
  // Push fresh values to ThingSpeak when a self-test or a cleaning cycle ends,
  // whoever started it, so the phone shows the result without waiting.
  bool st = Diagnostics::selfTestActive(), cl = Cleaner::busy();
  if ((wasSelfTest && !st) || (wasCleaning && !cl)) Cloud::requestUpload();
  wasSelfTest = st;
  wasCleaning = cl;

  switch (step) {
    case Step::Idle:
      return;
    case Step::SelfTest:
      if (st) return;
      rSelf = app.selfTest;
      startTilt();
      return;
    case Step::TiltPos:
    case Step::TiltNeg:
    case Step::TiltHome:
      updateTilt();
      return;
    case Step::Wiper:
      if (cl) return;
      rWiper = "PASS: 1 pass in " + String((millis() - wiperStart) / 1000) + " s (check the foam touched both rows)";
      if (!full) report(rWiper);
      finishAll();
      return;
  }
}

}  // namespace Tests
