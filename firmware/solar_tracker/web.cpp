#include "web.h"
#include "config.h"
#include "app_state.h"
#include "tracker.h"
#include "cleaner.h"
#include "diagnostics.h"
#include "dashboard_html.h"
#include <WiFi.h>
#include <WebServer.h>

namespace {
  WebServer server(80);

  // Our strings never contain control characters, but escape quotes and backslashes anyway.
  void appendJsonString(String &out, const char *s) {
    out += '"';
    for (; *s; s++) {
      if (*s == '"' || *s == '\\') out += '\\';
      out += *s;
    }
    out += '"';
  }

  void appendNumber(String &out, const char *key, float v, int decimals) {
    out += '"'; out += key; out += "\":";
    if (isnan(v)) out += "null"; else out += String(v, decimals);
    out += ',';
  }

  void sendStatus() {
    const SensorData &s = app.sensors;
    const DiagReport &d = app.diag;
    const TrackerStatus &t = Tracker::status();

    String j;
    j.reserve(2048);
    j += '{';
    j += "\"uptime_s\":"; j += millis() / 1000; j += ',';
    appendNumber(j, "voltage", s.busVoltage, 2);
    appendNumber(j, "current", s.current, 3);
    appendNumber(j, "power", s.power, 2);
    appendNumber(j, "panel_temp", s.panelTempC, 1);
    j += "\"ldr_east\":"; j += t.ldrEast; j += ',';
    j += "\"ldr_west\":"; j += t.ldrWest; j += ',';
    appendNumber(j, "imbalance", t.imbalance, 3);
    j += "\"tilt\":"; j += t.tiltDeg; j += ',';
    j += "\"mode\":"; appendJsonString(j, Tracker::modeName(t.mode)); j += ',';
    j += "\"state\":"; appendJsonString(j, Tracker::stateName(t.state)); j += ',';
    j += "\"wiper_busy\":"; j += Cleaner::busy() ? "true" : "false"; j += ',';
    j += "\"clean_runs\":"; j += Cleaner::runCount(); j += ',';
    j += "\"auto_clean\":"; j += app.autoClean ? "true" : "false"; j += ',';
    j += "\"calibrated\":"; j += d.calibrated ? "true" : "false"; j += ',';
    appendNumber(j, "expected_power", d.expectedPower, 2);
    appendNumber(j, "soiling_ratio", d.soilingRatio, 2);
    j += "\"worst\":"; appendJsonString(j, Diagnostics::severityName(d.worst)); j += ',';
    j += "\"findings\":[";
    for (int i = 0; i < d.count; i++) {
      if (i) j += ',';
      j += "{\"code\":"; appendJsonString(j, d.items[i].code);
      j += ",\"sev\":";  appendJsonString(j, Diagnostics::severityName(d.items[i].sev));
      j += ",\"msg\":";  appendJsonString(j, d.items[i].message);
      j += '}';
    }
    j += "],\"log\":[";
    for (int i = 0; i < app.log.count; i++) {
      if (i) j += ',';
      appendJsonString(j, app.log.get(i));
    }
    j += "],\"rssi\":"; j += app.stationMode ? WiFi.RSSI() : 0;
    j += '}';

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", j);
  }

  void sendResult(int code, const char *msg) {
    String j = "{\"ok\":";
    j += (code == 200) ? "true" : "false";
    j += ",\"message\":";
    appendJsonString(j, msg);
    j += '}';
    server.send(code, "application/json", j);
  }

  void handleClean() {
    if (Cleaner::start("manual")) {
      app.log.add("Cleaning cycle started from dashboard");
      sendResult(200, "Cleaning started");
    } else {
      sendResult(409, "A cleaning cycle is already running");
    }
  }

  void handleAutoClean() {
    if (!server.hasArg("on")) { sendResult(400, "Missing ?on=1 or ?on=0"); return; }
    app.autoClean = server.arg("on") == "1";
    app.log.add("Auto-clean %s", app.autoClean ? "enabled" : "disabled");
    sendResult(200, app.autoClean ? "Auto-clean on" : "Auto-clean off");
  }

  void handleCalibrate() {
    const char *reason = "";
    bool ok = Diagnostics::calibrate(app.sensors, Tracker::status(), &reason);
    app.log.add("Calibrate: %s", reason);
    sendResult(ok ? 200 : 409, reason);
  }

  void handleMode() {
    String m = server.arg("m");
    if (m == "auto")        Tracker::setMode(TrackerMode::Auto);
    else if (m == "manual") Tracker::setMode(TrackerMode::Manual);
    else { sendResult(400, "Use ?m=auto or ?m=manual"); return; }
    app.log.add("Tracker mode: %s", m.c_str());
    sendResult(200, m.c_str());
  }

  void handleTilt() {
    if (!server.hasArg("deg")) { sendResult(400, "Missing ?deg="); return; }
    int deg = constrain(server.arg("deg").toInt(), -TILT_LIMIT_DEG, TILT_LIMIT_DEG);
    Tracker::setManualTilt(deg);
    sendResult(200, "Tilt set (manual mode)");
  }
}

namespace Web {

void begin() {
  server.on("/", HTTP_GET, [] {
    server.send_P(200, "text/html", DASHBOARD_HTML);
  });
  server.on("/api/status",    HTTP_GET,  sendStatus);
  server.on("/api/clean",     HTTP_POST, handleClean);
  server.on("/api/autoclean", HTTP_POST, handleAutoClean);
  server.on("/api/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/mode",      HTTP_POST, handleMode);
  server.on("/api/tilt",      HTTP_POST, handleTilt);
  server.onNotFound([] { sendResult(404, "Not found"); });
  server.begin();
}

void handle() { server.handleClient(); }

}  // namespace Web
