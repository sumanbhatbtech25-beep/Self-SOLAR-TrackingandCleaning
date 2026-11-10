// commands.h — text commands, shared by the Serial Monitor and the dashboard.
#pragma once
#include <Arduino.h>

namespace Commands {
  // Runs one command line, e.g. "label E2_light_s1" or "clean". Returns a reply.
  String run(String line);
  void printHelp();
}
