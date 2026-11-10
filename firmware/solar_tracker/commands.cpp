#include "commands.h"
#include "config.h"
#include "app_state.h"
#include "tracker.h"
#include "cleaner.h"
#include "diagnostics.h"
#include "logger.h"
#include "tests.h"
#include "cloud.h"

namespace {
  uint32_t restartAt = 0;

  // Labels and marks go into a CSV column: keep them to one safe word.
  String clean(String s) {
    s.trim();
    String out;
    for (size_t i = 0; i < s.length() && out.length() < 40; i++) {
      char c = s[i];
      out += (isalnum(c) || c == '-' || c == '.') ? c : '_';
    }
    return out;
  }
}

namespace Commands {

void printHelp() {
  Serial.println(F("# Commands (type in the Serial Monitor, Newline on):"));
  Serial.println(F("#   label <name>     tag every following CSV line, e.g. label E2_light_s1"));
  Serial.println(F("#   mark <event>     tag the next CSV line once, e.g. mark inject_ldr"));
  Serial.println(F("#   clean            run one wiper cycle (panel levelled first)"));
  Serial.println(F("#   auto on|off      automatic cleaning when dust is confirmed (default off)"));
  Serial.println(F("#   calibrate        store the clean-panel baseline (clean panel, steady light)"));
  Serial.println(F("#   selftest         relay self-test of both current sensors"));
  Serial.println(F("#   mode auto        sun / torch tracking"));
  Serial.println(F("#   mode fixed [deg] hold the tilt (default 0 = flat), for experiment E6"));
  Serial.println(F("#   tilt <deg>       manual tilt angle from flat (-60..60)"));
  Serial.println(F("#   wiper <deg>      move the wiper servo to an angle (testing only)"));
  Serial.println(F("#   test sensors     check every sensor once"));
  Serial.println(F("#   test tilt        tilt sweep: +limit, -limit, flat; LDR balance must change"));
  Serial.println(F("#   test wiper       one wiper pass"));
  Serial.println(F("#   test full        sensors, current self-test, tilt and wiper in a row"));
  Serial.println(F("#   stop             stop a test or the wiper (foam returns to park)"));
  Serial.println(F("#   upload           send readings to ThingSpeak now"));
  Serial.println(F("#   reboot           restart the ESP32"));
  Serial.println(F("#   header           print the CSV header again"));
}

String runLine(String line) {
  line.trim();
  int sp = line.indexOf(' ');
  String cmd = sp < 0 ? line : line.substring(0, sp);
  String arg = sp < 0 ? "" : line.substring(sp + 1);
  cmd.toLowerCase();
  arg.trim();
  String msg;

  if (cmd == "label") {
    app.label = arg.length() ? clean(arg) : "none";
    app.mark = "label_" + app.label;
    return "Label: " + app.label;
  }
  if (cmd == "mark") {
    app.mark = clean(arg);
    return "Mark: " + app.mark;
  }
  if (cmd == "clean") {
    if (Tests::busy()) return "A test is running: wait, or send stop";
    return Cleaner::start() ? "Cleaning started" : "Wiper already running";
  }
  if (cmd == "test") {
    String which = arg; which.toLowerCase();
    Tests::start(which, msg);
    return msg;
  }
  if (cmd == "stop") {
    bool any = Tests::busy() || Cleaner::busy();
    Tests::stop();
    Cleaner::stop();
    return any ? "Stopped: wiper returns to park, tracking resumes" : "Nothing running";
  }
  if (cmd == "upload") { Cloud::requestUpload(); return Cloud::enabled() ? "Upload requested" : "ThingSpeak is off"; }
  if (cmd == "reboot" || cmd == "restart") { restartAt = millis() | 1; return "Restarting in 2 s"; }
  if (cmd == "auto") {
    app.autoClean = (arg == "on");
    return String("Auto-clean ") + (app.autoClean ? "on" : "off");
  }
  if (cmd == "calibrate") {
    if (Tests::busy()) return "A test is running: wait, or send stop";
    Diagnostics::calibrate(msg);
    return msg;
  }
  if (cmd == "selftest")  { Diagnostics::startSelfTest(msg); return msg; }
  if (cmd == "mode") {
    String m = arg; int deg = 0;
    int s2 = arg.indexOf(' ');
    if (s2 > 0) { m = arg.substring(0, s2); deg = arg.substring(s2 + 1).toInt(); }
    if (m == "auto")  { Tracker::setMode(TrackMode::Auto);        return "Tracking: auto"; }
    if (m == "fixed") { Tracker::setMode(TrackMode::Fixed, deg);  return "Tracking: fixed at " + String(deg) + " deg"; }
    return "Use: mode auto | mode fixed [deg]";
  }
  if (cmd == "tilt") {
    if (Tests::busy() || Cleaner::busy()) return "Busy: send stop first";
    int deg = constrain(arg.toInt(), -TILT_RANGE_DEG, TILT_RANGE_DEG);
    Tracker::setMode(TrackMode::Manual, deg);
    return "Tilt (manual): " + String(deg) + " deg";
  }
  if (cmd == "wiper") {
    if (Cleaner::busy()) return "Wiper busy";
    Cleaner::moveTest(arg.toInt());
    return "Wiper -> " + String(arg.toInt());
  }
  if (cmd == "header") { Logger::printHeader(); return "Header printed"; }
  if (cmd == "help" || cmd == "?") { printHelp(); return "Help printed"; }
  return "Unknown command: " + cmd + " (type help)";
}

String run(String line) {
  String reply = runLine(line);
  line.trim();
  app.lastCmd = line + " -> " + reply;
  return reply;
}

void service() {
  if (restartAt && millis() - restartAt > 2000) ESP.restart();
}

}  // namespace Commands
