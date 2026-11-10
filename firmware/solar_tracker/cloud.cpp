#include "cloud.h"
#include "config.h"
#include "app_state.h"
#include "tracker.h"
#include "cleaner.h"
#include "relays.h"
#include "commands.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef TS_WRITE_KEY
#define TS_WRITE_KEY ""
#endif
#ifndef TS_HEALTH_WRITE_KEY
#define TS_HEALTH_WRITE_KEY ""
#endif
#ifndef TS_TALKBACK_ID
#define TS_TALKBACK_ID ""
#endif
#ifndef TS_TALKBACK_KEY
#define TS_TALKBACK_KEY ""
#endif

namespace {
  // Readings copied by the main loop every second and read by the cloud task.
  // Plain numbers and fixed char buffers only: no String is shared between the cores.
  struct Snap {
    float live[8];
    float health[8];
    char liveStatus[200];
    char healthStatus[251];        // ThingSpeak status: 255 characters at most
  };
  const int LIVE_DEC[8]   = {3, 2, 1, 0, 0, 0, 0, 1};
  const int HEALTH_DEC[8] = {1, 1, 0, 2, 1, 1, 1, 0};
  constexpr size_t CMD_LEN = 48;

  Snap snap;
  SemaphoreHandle_t lock = nullptr;
  QueueHandle_t cmdQueue = nullptr;
  volatile bool running = false;
  volatile bool snapReady = false;
  volatile bool forceLive = false, forceHealth = false;
  volatile uint32_t okLiveMs = 0, okHealthMs = 0;
  char err[64] = "";               // last error (under lock)
  String note = "off";             // why the cloud is off (main loop only)

  void setErr(const String &e) {
    if (xSemaphoreTake(lock, pdMS_TO_TICKS(50)) != pdTRUE) return;
    strlcpy(err, e.c_str(), sizeof(err));
    xSemaphoreGive(lock);
  }

  String urlEncode(const char *s) {
    String out;
    const char *hex = "0123456789ABCDEF";
    for (; *s; s++) {
      char c = *s;
      if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') out += c;
      else { out += '%'; out += hex[(c >> 4) & 15]; out += hex[c & 15]; }
    }
    return out;
  }

  bool send(const char *key, const float *f, const int *dec, const char *status) {
    String url = "http://api.thingspeak.com/update?api_key=";
    url += key;
    for (int i = 0; i < 8; i++) {
      if (isnan(f[i]) || isinf(f[i])) continue;          // missing reading: leave the field empty
      url += "&field"; url += (i + 1); url += '='; url += String(f[i], dec[i]);
    }
    url += "&status=";
    url += urlEncode(status);
    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(url)) { setErr("could not start HTTP"); return false; }
    int code = http.GET();
    String body = code > 0 ? http.getString() : String("");
    http.end();
    // ThingSpeak replies with the new entry number, or 0 if it refused the update.
    if (code == 200 && body.toInt() > 0) return true;
    setErr(code == 200 ? String("refused: wrong key or too fast") : "HTTP error " + String(code));
    return false;
  }

  void pollTalkBack() {
    String url = "http://api.thingspeak.com/talkbacks/" TS_TALKBACK_ID "/commands/execute.json?api_key=" TS_TALKBACK_KEY;
    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(url)) return;
    int code = http.POST("");
    String body = code > 0 ? http.getString() : String("");
    http.end();
    if (code != 200) { setErr("TalkBack HTTP " + String(code)); return; }
    // Reply: {"id":...,"command_string":"CLEAN",...}  or  {} / empty when the queue is empty.
    int k = body.indexOf("\"command_string\":\"");
    if (k < 0) return;
    k += 18;
    int e = body.indexOf('"', k);
    if (e <= k) return;
    char cmd[CMD_LEN] = {0};
    body.substring(k, e).toCharArray(cmd, sizeof(cmd));
    xQueueSend(cmdQueue, cmd, 0);
  }

  void task(void *) {
    uint32_t lastLive = 0, lastHealth = 0, lastTb = 0;
    bool sentLive = false, sentHealth = false;
    static Snap s;
    for (;;) {
      vTaskDelay(pdMS_TO_TICKS(250));
      if (WiFi.status() != WL_CONNECTED) { setErr("Wi-Fi lost, reconnecting"); continue; }
      if (!snapReady) continue;
      uint32_t now = millis();

      bool liveDue = strlen(TS_WRITE_KEY) &&
                     (!sentLive || now - lastLive >= (forceLive ? CLOUD_MIN_GAP_MS : CLOUD_UPLOAD_MS));
      bool healthDue = strlen(TS_HEALTH_WRITE_KEY) &&
                       (!sentHealth || now - lastHealth >= (forceHealth ? CLOUD_MIN_GAP_MS : CLOUD_HEALTH_MS));
      if (liveDue || healthDue) {
        if (xSemaphoreTake(lock, pdMS_TO_TICKS(100)) != pdTRUE) continue;
        s = snap;
        xSemaphoreGive(lock);
      }
      if (liveDue) {
        lastLive = now; sentLive = true; forceLive = false;
        if (send(TS_WRITE_KEY, s.live, LIVE_DEC, s.liveStatus)) { okLiveMs = millis(); setErr(""); }
      }
      if (healthDue) {
        lastHealth = millis(); sentHealth = true; forceHealth = false;
        if (send(TS_HEALTH_WRITE_KEY, s.health, HEALTH_DEC, s.healthStatus)) { okHealthMs = millis(); setErr(""); }
      }
      if (strlen(TS_TALKBACK_ID) && strlen(TS_TALKBACK_KEY) && millis() - lastTb >= CLOUD_TALKBACK_MS) {
        lastTb = millis();
        pollTalkBack();
      }
    }
  }

  // TalkBack names -> Serial commands. Anything else is passed on in lower case.
  String translate(String c) {
    c.trim();
    String u = c; u.toUpperCase();
    if (u == "CLEAN")         return "clean";
    if (u == "STOP")          return "stop";
    if (u == "SENSOR_TEST")   return "test sensors";
    if (u == "TILT_TEST")     return "test tilt";
    if (u == "WIPER_TEST")    return "test wiper";
    if (u == "SELF_TEST")     return "selftest";
    if (u == "FULL_TEST")     return "test full";
    if (u == "CALIBRATE")     return "calibrate";
    if (u == "AUTOCLEAN_ON")  return "auto on";
    if (u == "AUTOCLEAN_OFF") return "auto off";
    if (u == "TRACK_AUTO")    return "mode auto";
    if (u == "PARK_FLAT")     return "mode fixed 0";
    if (u == "UPLOAD_NOW")    return "upload";
    if (u == "REBOOT")        return "reboot";
    c.toLowerCase();
    return c;
  }

  // CSV-safe tag for the log's mark column.
  String tag(const char *cmd) {
    String out = "phone_";
    for (; *cmd && out.length() < 40; cmd++) out += isalnum((unsigned char)*cmd) ? *cmd : '_';
    return out;
  }

  void put(char *dst, size_t n, const String &v) { strlcpy(dst, v.c_str(), n); }
}

namespace Cloud {

bool enabled() { return running; }

void requestUpload() { forceLive = true; forceHealth = true; }

String lastResult() {
  if (!running) return note;
  auto part = [](const char *name, bool on, uint32_t okMs) -> String {
    if (!on) return String(name) + " off";
    if (!okMs) return String(name) + " waiting";
    return String(name) + " OK " + String((millis() - okMs) / 1000) + " s ago";
  };
  String r = part("live", strlen(TS_WRITE_KEY), okLiveMs) + ", " +
             part("health", strlen(TS_HEALTH_WRITE_KEY), okHealthMs) + ", phone " +
             (strlen(TS_TALKBACK_ID) && strlen(TS_TALKBACK_KEY) ? "on" : "off");
  char e[64] = "";
  if (xSemaphoreTake(lock, pdMS_TO_TICKS(20)) == pdTRUE) {
    strlcpy(e, err, sizeof(e));
    xSemaphoreGive(lock);
  }
  if (e[0]) { r += " (last error: "; r += e; r += ')'; }
  return r;
}

void begin() {
  if (!strlen(TS_WRITE_KEY) && !strlen(TS_HEALTH_WRITE_KEY) && !strlen(TS_TALKBACK_ID)) {
    note = "off (no keys in secrets.h)";
    Serial.println(F("# ThingSpeak: off (no keys in secrets.h)"));
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    note = "off (not on Wi-Fi at boot)";
    Serial.println(F("# ThingSpeak: off (not on Wi-Fi at boot)"));
    return;
  }
  lock = xSemaphoreCreateMutex();
  cmdQueue = xQueueCreate(6, CMD_LEN);
  if (!lock || !cmdQueue) { note = "off (out of memory)"; return; }
  running = true;
  xTaskCreatePinnedToCore(task, "cloud", 8192, nullptr, 1, nullptr, 0);
  Serial.printf("# ThingSpeak: live %s, health %s, phone commands %s\n",
                strlen(TS_WRITE_KEY) ? "on" : "off", strlen(TS_HEALTH_WRITE_KEY) ? "on" : "off",
                strlen(TS_TALKBACK_ID) && strlen(TS_TALKBACK_KEY) ? "on" : "off");
}

void snapshot() {
  if (!running) return;
  const SensorData &s = app.s;
  const Soiling &so = app.soil;
  const TrackStatus &t = Tracker::status();
  static Snap n;

  n.live[0] = s.powerW;
  n.live[1] = s.panelV;
  n.live[2] = s.currentA * 1000.0f;
  n.live[3] = s.photoMv;
  n.live[4] = s.ldrA;
  n.live[5] = s.ldrB;
  n.live[6] = t.angle;
  n.live[7] = so.valid ? so.ratioLight * 100.0f : NAN;        // only when the dust check is valid

  bool onLoad = so.calibrated && Relays::panelConnected() && !Cleaner::busy();
  int faultCount = 0;
  if (app.faults.length()) {
    faultCount = 1;
    for (size_t i = 0; i < app.faults.length(); i++) if (app.faults[i] == '|') faultCount++;
  }
  n.health[0] = (s.panelC > -100) ? s.panelC : NAN;             // -127 = DS18B20 missing
  n.health[1] = s.airC;
  n.health[2] = s.humidity;
  n.health[3] = s.supplyV;
  n.health[4] = s.inaOk ? s.inaA * 1000.0f : NAN;
  n.health[5] = s.acsA * 1000.0f;
  n.health[6] = onLoad ? so.ratioNaive * 100.0f : NAN;
  n.health[7] = faultCount;

  String f = app.faults;
  f.replace("|", ", ");
  put(n.liveStatus, sizeof(n.liveStatus), f.length() ? f : String("OK"));

  String h = String("mode ") + Tracker::modeName(t.mode) + ", auto-clean " + (app.autoClean ? "on" : "off") +
             ", cleanings " + String(Cleaner::runCount()) + (Cleaner::busy() ? ", wiping" : "") +
             " | test: " + app.testResult + " | self-test: " + app.selfTest;
  if (app.lastCmd.length()) h += " | cmd: " + app.lastCmd;
  put(n.healthStatus, sizeof(n.healthStatus), h);

  if (xSemaphoreTake(lock, pdMS_TO_TICKS(10)) != pdTRUE) return;   // try again next second
  snap = n;
  xSemaphoreGive(lock);
  snapReady = true;
}

void handleCommands() {
  if (!cmdQueue) return;
  char cmd[CMD_LEN];
  while (xQueueReceive(cmdQueue, cmd, 0) == pdTRUE) {
    String c = translate(String(cmd));
    if (!c.length()) continue;
    app.mark = tag(cmd);
    String reply = Commands::run(c);
    app.lastCmd = String("phone ") + cmd + " -> " + reply;
    Serial.print(F("# Phone: "));
    Serial.print(cmd);
    Serial.print(F(" -> "));
    Serial.println(reply);
    requestUpload();
  }
}

}  // namespace Cloud
