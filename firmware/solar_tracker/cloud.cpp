#include "cloud.h"
#include "config.h"
#include "app_state.h"
#include "tracker.h"
#include "commands.h"
#include <WiFi.h>
#include <HTTPClient.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef TS_WRITE_KEY
#define TS_WRITE_KEY ""
#endif
#ifndef TS_TALKBACK_ID
#define TS_TALKBACK_ID ""
#endif
#ifndef TS_TALKBACK_KEY
#define TS_TALKBACK_KEY ""
#endif

namespace {
  QueueHandle_t cmdQueue = nullptr;
  volatile bool running = false;
  String result = "off";
  uint32_t lastOkMs = 0;

  void add(String &url, int field, float v, int decimals) {
    if (isnan(v)) return;
    url += "&field"; url += field; url += '='; url += String(v, decimals);
  }

  String urlEncode(const String &s) {
    String out;
    const char *hex = "0123456789ABCDEF";
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      if (isalnum(c) || c == '-' || c == '_' || c == '.') out += c;
      else { out += '%'; out += hex[(c >> 4) & 15]; out += hex[c & 15]; }
    }
    return out;
  }

  void upload() {
    const SensorData &s = app.s;
    String url = "http://api.thingspeak.com/update?api_key=" TS_WRITE_KEY;
    add(url, 1, s.powerW, 3);
    add(url, 2, s.currentA * 1000.0f, 1);
    add(url, 3, s.photoMv, 0);
    add(url, 4, app.soil.ratioLight * 100.0f, 1);
    add(url, 5, Tracker::status().angle, 0);
    if (s.panelC > -100) add(url, 6, s.panelC, 1);
    add(url, 7, s.airC, 1);
    add(url, 8, s.humidity, 0);
    String status = app.faults.length() ? app.faults : String("OK");
    url += "&status=" + urlEncode(status);
    HTTPClient http;
    http.setTimeout(4000);
    http.begin(url);
    int code = http.GET();
    String body = http.getString();
    http.end();
    // ThingSpeak replies with the new entry number, or 0 if it rejected the update.
    if (code == 200 && body.toInt() > 0) { result = "upload OK"; lastOkMs = millis(); }
    else result = "upload failed (" + String(code) + ")";
  }

  void pollTalkBack() {
    String url = "http://api.thingspeak.com/talkbacks/" TS_TALKBACK_ID "/commands/execute.json?api_key=" TS_TALKBACK_KEY;
    HTTPClient http;
    http.setTimeout(4000);
    http.begin(url);
    int code = http.POST("");
    String body = http.getString();
    http.end();
    if (code != 200) return;
    // Reply: {"id":…,"command_string":"CLEAN",…}  or  {} when the queue is empty.
    int k = body.indexOf("\"command_string\":\"");
    if (k < 0) return;
    k += 18;
    int e = body.indexOf('"', k);
    if (e < 0) return;
    char cmd[32] = {0};
    body.substring(k, e).toCharArray(cmd, sizeof(cmd));
    xQueueSend(cmdQueue, cmd, 0);
  }

  void task(void *) {
    uint32_t lastUp = 0, lastTb = 0;
    for (;;) {
      if (WiFi.status() == WL_CONNECTED) {
        if (strlen(TS_WRITE_KEY) && (lastUp == 0 || millis() - lastUp >= CLOUD_UPLOAD_MS)) {
          lastUp = millis();
          upload();
        }
        if (strlen(TS_TALKBACK_ID) && strlen(TS_TALKBACK_KEY) && millis() - lastTb >= CLOUD_TALKBACK_MS) {
          lastTb = millis();
          pollTalkBack();
        }
      } else {
        result = "no Wi-Fi (hotspot mode)";
      }
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }

  // TalkBack names -> the same commands as the Serial Monitor.
  String translate(const String &c) {
    if (c == "CLEAN")         return "clean";
    if (c == "AUTOCLEAN_ON")  return "auto on";
    if (c == "AUTOCLEAN_OFF") return "auto off";
    if (c == "SELF_TEST")     return "selftest";
    if (c == "CALIBRATE")     return "calibrate";
    if (c == "TRACK_AUTO")    return "mode auto";
    if (c == "PARK_FLAT")     return "mode fixed 0";
    return "";
  }
}

namespace Cloud {

bool enabled() { return running; }

String lastResult() {
  if (!running) return result;
  if (lastOkMs == 0) return result;
  return result + ", last upload " + String((millis() - lastOkMs) / 1000) + " s ago";
}

void begin() {
  if (!strlen(TS_WRITE_KEY) && !strlen(TS_TALKBACK_ID)) { result = "off (no keys in secrets.h)"; return; }
  if (WiFi.status() != WL_CONNECTED) { result = "off (not on Wi-Fi)"; return; }
  cmdQueue = xQueueCreate(4, 32);
  running = true;
  result = "starting";
  xTaskCreatePinnedToCore(task, "cloud", 8192, nullptr, 1, nullptr, 0);
  Serial.println(F("# ThingSpeak: on"));
}

void handleCommands() {
  if (!cmdQueue) return;
  char cmd[32];
  while (xQueueReceive(cmdQueue, cmd, 0) == pdTRUE) {
    String c = translate(String(cmd));
    if (!c.length()) { Serial.printf("# TalkBack: unknown command %s\n", cmd); continue; }
    app.mark = String("phone_") + cmd;
    Serial.print("# TalkBack: ");
    Serial.println(Commands::run(c));
  }
}

}  // namespace Cloud
