// logger.h — one CSV line per second on Serial (115200 baud).
// Every other Serial message starts with '#', so analysis scripts can skip it.
#pragma once
#include <Arduino.h>

namespace Logger {
  void printHeader();
  void printRow();
}
