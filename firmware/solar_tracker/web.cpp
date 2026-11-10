#include "web.h"
#include "config.h"
#include "app_state.h"
#include "tracker.h"
#include "cleaner.h"
#include "relays.h"
#include "diagnostics.h"
#include "commands.h"
#include "dashboard_html.h"
#include "cloud.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace {
  WebServer server(80);
  String addr = "-";

  void jstr(String &out, const char *key, const String &v) {
    out += '"'; out += key; out += "\":\"";
    for (size_t i = 0; i < v.length(); i++) {
      char c = v[i];
      if (c == '"' || c == '\\') out += '\\';
      out += c;
    }
    out += "\",";
  }

  void jnum(String &out, const char *key, float v, int decimals) {
    out += '"'; out += key; out += "\":";
    if (isnan(v)) out += "null"; else out += String(v, decimals);
    out += ',';
  }

  void sendStatus() {
    const SensorData &s = app.s;
    const TrackStatus &t = Tracker::status();
    String j;
    j.reserve(1400);
    j += '{';
    jnum(j, "uptime_s", millis() / 1000.0f, 0);
    jnum(j, "panel_v", s.panelV, 2);
    jnum(j, "current_a", s.currentA, 3);
    jnum(j, "ina_a", s.inaA, 3);
    jnum(j, "acs_a", s.acsA, 3);
    jnum(j, "power_w", s.powerW, 2);
    jnum(j, "photo_mv", s.photoMv, 0);
    jnum(j, "ldr_a", s.ldrA, 0);
    jnum(j, "ldr_b", s.ldrB, 0);
    jnum(j, "imbalance", t.imbalance, 3);
    jnum(j, "tilt", t.angle, 0);
    jnum(j, "panel_c", s.panelC, 1);
    jnum(j, "air_c", s.airC, 1);
    jnum(j, "humidity", s.humidity, 0);
    jnum(j, "supply_v", s.supplyV, 2);
    jnum(j, "soil_light", app.soil.ratioLight, 2);
    jnum(j, "soil_naive", app.soil.ratioNaive, 2);
    jstr(j, "mode", Tracker::modeName(t.mode));
    jstr(j, "trusted", app.trusted == Sensor::INA219 ? "INA219" : "ACS712");
    jstr(j, "label", app.label);
    jstr(j, "selftest", app.selfTest);
    jstr(j, "cloud", Cloud::lastResult());
    j += "\"calibrated\":"; j += app.soil.calibrated ? "true" : "false"; j += ',';
    j += "\"auto_clean\":"; j += app.autoClean ? "true" : "false"; j += ',';
    j += "\"wiper\":"; j += Cleaner::busy() ? "true" : "false"; j += ',';
    j += "\"demo\":"; j += DEMO_MODE ? "true" : "false"; j += ',';
    j += "\"faults\":[";
    String f = app.faults;
    bool first = true;
    while (f.length()) {
      int bar = f.indexOf('|');
      String code = bar < 0 ? f : f.substring(0, bar);
      f = bar < 0 ? "" : f.substring(bar + 1);
      if (!first) j += ',';
      first = false;
      j += "{\"code\":\""; j += code; j += "\",\"msg\":\""; j += Diagnostics::describe(code); j += "\"}";
    }
    j += "]}";
    server.send(200, "application/json", j);
  }

  void runCmd() {
    String reply = Commands::run(server.arg("c"));
    String j = "{";
    jstr(j, "msg", reply);
    j.remove(j.length() - 1);
    j += '}';
    Serial.print("# web: ");
    Serial.println(reply);
    server.send(200, "application/json", j);
  }
}

namespace Web {

void begin() {
  bool connected = false;
#ifdef WIFI_SSID
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("# Wi-Fi: connecting to " WIFI_SSID);
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print('.'); }
  Serial.println();
  connected = WiFi.status() == WL_CONNECTED;
  if (connected) addr = WiFi.localIP().toString();
#endif
  if (!connected) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    addr = WiFi.softAPIP().toString();
    Serial.println("# Wi-Fi: own hotspot " AP_SSID " (password " AP_PASSWORD ")");
  }
  if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
  Serial.println("# Dashboard: http://" + addr + "/  (or http://" MDNS_NAME ".local/)");

  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", DASHBOARD_HTML); });
  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/cmd", HTTP_POST, runCmd);
  server.begin();
}

void handle() { server.handleClient(); }
String address() { return addr; }

}  // namespace Web
