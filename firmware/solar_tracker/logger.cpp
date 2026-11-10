#include "logger.h"
#include "app_state.h"
#include "tracker.h"
#include "cleaner.h"
#include "relays.h"
#include "diagnostics.h"

namespace {
  // Empty field for "not available", so pandas reads it as NaN.
  void num(String &out, float v, int decimals) {
    if (!isnan(v)) out += String(v, decimals);
    out += ',';
  }
}

namespace Logger {

void printHeader() {
  Serial.println(F("ms,label,mark,mode,tilt_deg,ldr_a,ldr_b,imbalance,photo_mv,panel_v,ina_a,acs_a,"
                   "current_a,trusted,power_w,soil_naive,soil_light,rule_a,rule_b,soil_valid,"
                   "panel_c,air_c,humidity,supply_v,wiper,panel_connected,servo_power,selftest,faults"));
}

void printRow() {
  const SensorData &s = app.s;
  const Soiling &so = app.soil;
  const TrackStatus &t = Tracker::status();
  String r;
  r.reserve(320);
  r += millis(); r += ',';
  r += app.label; r += ',';
  r += app.mark; r += ',';
  r += Tracker::modeName(t.mode); r += ',';
  r += t.angle; r += ',';
  r += s.ldrA; r += ',';
  r += s.ldrB; r += ',';
  num(r, t.imbalance, 3);
  num(r, s.photoMv, 0);
  num(r, s.panelV, 3);
  num(r, s.inaA, 4);
  num(r, s.acsA, 4);
  num(r, s.currentA, 4);
  r += (app.trusted == Sensor::INA219 ? "ina" : "acs"); r += ',';
  num(r, s.powerW, 3);
  num(r, so.ratioNaive, 3);
  num(r, so.ratioLight, 3);
  r += so.ruleA ? '1' : '0'; r += ',';
  r += so.ruleB ? '1' : '0'; r += ',';
  r += so.valid ? '1' : '0'; r += ',';
  num(r, s.panelC, 2);
  num(r, s.airC, 1);
  num(r, s.humidity, 0);
  num(r, s.supplyV, 2);
  r += Cleaner::busy() ? '1' : '0'; r += ',';
  r += Relays::panelConnected() ? '1' : '0'; r += ',';
  r += Relays::servoPower() ? '1' : '0'; r += ',';
  r += Diagnostics::selfTestActive() ? '1' : '0'; r += ',';
  r += app.faults;
  Serial.println(r);
  app.mark = "";            // a mark appears on exactly one line
}

}  // namespace Logger
