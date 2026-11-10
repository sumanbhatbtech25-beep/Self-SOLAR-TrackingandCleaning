// app_state.h — readings and settings shared by all modules.
#pragma once
#include <Arduino.h>

struct SensorData {
  // Panel line
  bool  inaOk = false;
  float panelV = NAN;      // INA219 bus voltage (V)
  float inaA = NAN;        // INA219 current (A)
  float acsA = NAN;        // ACS712 current (A)
  float acsPinMv = NAN;    // ACS712 voltage at GPIO 36 (mV)
  float currentA = NAN;    // current from the trusted sensor
  float powerW = NAN;
  // Light
  float photoMv = NAN;     // photo transistor voltage (mV): the light reference
  int   ldrA = 0;          // LDR pair A light level (0-4095, higher = brighter)
  int   ldrB = 0;
  // Temperature, air, supply
  float panelC = NAN;      // DS18B20 (-127 = not connected)
  float airC = NAN;        // DHT11
  float humidity = NAN;
  float supplyV = NAN;
};

enum class Sensor : uint8_t { INA219, ACS712 };

struct Soiling {
  bool  calibrated = false;
  float k = NAN;           // clean-panel current per mV of light (A/mV)
  float baseA = NAN;       // clean-panel current at calibration (A)
  float ratioLight = NAN;  // Rule B: I / (k * light)
  float ratioNaive = NAN;  // Rule A: I / baseline current
  bool  ruleA = false;     // Rule A below threshold on this reading
  bool  ruleB = false;     // Rule B below threshold on this reading
  bool  valid = false;     // enough light, not saturated, not wiping, not self-testing
};

struct AppState {
  SensorData s;
  Soiling soil;
  Sensor trusted = Sensor::INA219;
  bool autoClean = false;      // off by default so experiments are not disturbed
  String label = "none";       // experiment label, saved on every CSV line
  String mark = "";            // one-shot event marker, printed on the next CSV line
  String faults = "";          // active fault codes, '|' separated
  String selfTest = "not run"; // last self-test result
  String testResult = "none";  // last remote test (sensors / tilt / wiper / full)
  String lastCmd = "";         // last command and its reply, shown on the phone
};

extern AppState app;
