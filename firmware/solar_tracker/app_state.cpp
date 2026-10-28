#include "app_state.h"
#include <stdarg.h>

AppState app;

void EventLog::add(const char *fmt, ...) {
  uint32_t s = millis() / 1000;
  char msg[80];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  snprintf(lines[head], sizeof(lines[head]), "%02lu:%02lu:%02lu  %s",
           (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60), msg);
  head = (head + 1) % N;
  if (count < N) count++;
  Serial.printf("[event] %s\n", msg);
}
