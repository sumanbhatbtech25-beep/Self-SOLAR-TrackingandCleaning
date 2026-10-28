# Diagnostics rules

The firmware checks every rule once per second (`Diagnostics::evaluate` in [`diagnostics.cpp`](../firmware/solar_tracker/diagnostics.cpp)). Each finding has a stable code, a severity and a message. They appear on the dashboard, in `/api/status`, and in the event log when a warning or critical finding first appears.

All thresholds live in [`config.h`](../firmware/solar_tracker/config.h).

## Battery

| Code | Severity | Rule | What happens / what to check |
|---|---|---|---|
| `BATTERY_HIGH` | Critical | Battery > 15.0 V | Overcharging. Check the charge controller |
| `BATTERY_CRITICAL` | Critical | Battery < 11.8 V | After 30 s the board deep-sleeps in 10-minute naps until the battery is back above 12.3 V |
| `BATTERY_LOW` | Warning | Battery < 12.0 V | Charge is low. Clean the panel, check tracking |
| `BATTERY_FULL` | Info | Battery ≥ 13.6 V | Controller is limiting charge. The output checks below are paused |
| `BATTERY_SENSOR` | Warning | Battery reads < 5 V | Divider on GPIO 32 not connected, or set `BATTERY_ENABLED = false` for bench use |

## Charging path

These run only while the battery can still take charge. When it's full, the controller cuts the panel current on purpose.

| Code | Severity | Rule | What to check |
|---|---|---|---|
| `OPEN_CIRCUIT` | Critical | V > 19.5 V and I < 10 mA | INA219 VIN− to controller PV+ (wire 18) |
| `NO_PANEL_VOLTAGE` | Critical | Bright light but V < 1 V | Fuse F1, panel + wire (wires 16–17) |
| `NO_CHARGE` | Warning | Bright light, voltage present, but I < 10 mA | Controller PV terminals (wires 18–19), controller fault |
| `SOILING` | Warning | Panel current < 80 % of expected for this light | Run a cleaning cycle (or let auto-clean do it) |

## Sensors and tracking

| Code | Severity | Rule | What to check |
|---|---|---|---|
| `INA219_MISSING` | Critical | INA219 did not answer at boot | SDA/SCL on GPIO 21/22, 3V3 to VCC |
| `TEMP_SENSOR` | Warning | DS18B20 returns −127 or out-of-range | Data on GPIO 4, 4.7 kΩ pull-up |
| `OVERHEAT` | Critical | Panel temperature > 60 °C | Airflow behind the panel |
| `TILT_STALL` | Critical | Commanding moves for 4 s but the LDR imbalance hasn't dropped by 20 % | Servo power, linkage, binding axle |
| `LDR_MISMATCH` | Warning | In bright light, one LDR < 15 % of the other for 10 s | LDR covered, dirty or disconnected |
| `AT_LIMIT` | Info | Sun beyond the ±60° tilt range | Normal early and late in the day |
| `LDR_SATURATED` | Info | An LDR reads ≥ 4000 | Use a smaller divider resistor so brightness is measurable |
| `NIGHT` | Info | Average LDR < 150 | Tracker parks facing sunrise, then sleeps |
| `NOT_CALIBRATED` | Info | No clean-panel baseline saved yet | Clean the panel, then press **Calibrate** in full sun |
| `OK` | OK | No warning or critical findings | — |

## How the soiling check works

There is no pyranometer, so the LDRs act as the light reference. They sit outside the panel glass and don't collect the same dust.

The check uses panel **current**, not power. A PWM charge controller holds the panel close to battery voltage, so power rises and falls with the battery's state of charge. Current still follows sunlight and dust.

1. **Calibrate** on a clean panel in direct sun, with the battery below 13.6 V. The firmware stores `k = I_clean / LDR_average` in flash (survives reboots and deep sleep).
2. Every second it predicts `I_expected = k × LDR_average`.
3. `soiling_ratio = I / I_expected`. Below 0.80 raises `SOILING`.
4. After every cleaning cycle the baseline is re-recorded automatically 10 s later, provided the conditions are right.

**Limitations.** LDRs are not linear and saturate in full sun with a 10 kΩ divider, so the ratio is a relative indicator, not an absolute efficiency figure. Calibrate at roughly the time of day you expect to compare against. Re-recording after each clean also means a permanently damaged panel won't be flagged; that is a job for the TinyML phase.

## Auto-clean

When **Auto-clean** is on, a cycle starts if `SOILING` has been active for 1 minute (`AUTO_CLEAN_CONFIRM_MS`). At most one automatic cycle runs every 30 minutes (`AUTO_CLEAN_COOLDOWN_MS`). Tracking pauses while the blade is on the glass.

## Sleep

| Mode | Enters when | Wakes | Notes |
|---|---|---|---|
| Night | Dark, parked at the east limit for 5 min (`NIGHT_CONFIRM_MS`) | Every 15 min; goes straight back to sleep if still dark | Tilt is remembered across sleep. Dashboard offline. Turn off with `NIGHT_DEEP_SLEEP = false` |
| Low battery | Battery < 11.8 V for 30 s | Every 10 min; stays asleep until > 12.3 V | Protects the lead-acid battery from deep discharge |

## Testing the rules

| To trigger | Do this |
|---|---|
| `LDR_MISMATCH` | Cover one LDR with tape in bright light for 10 s |
| `NO_CHARGE` | Disconnect wire 19 (panel − at the controller) in sunlight |
| `NO_PANEL_VOLTAGE` | Pull fuse F1 in daylight |
| `TILT_STALL` | Unplug the tilt servo's signal wire while the sun is off-axis |
| `SOILING` | Calibrate clean, then dust the glass with flour or talc |
| `OVERHEAT` | Warm the probe with a hair dryer |
| `BATTERY_SENSOR` | Disconnect the 100 kΩ resistor from GPIO 32 |

The same faults, plus a full day and night of battery charge, can be tried without hardware in the [3D model](interactive/3d-model.html).
