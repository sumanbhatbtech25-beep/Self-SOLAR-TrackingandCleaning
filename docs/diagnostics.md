# Diagnostics rules

The firmware checks every rule once per second (`Diagnostics::evaluate` in [`diagnostics.cpp`](../firmware/solar_tracker/diagnostics.cpp)). Each finding has a stable code, a severity and a message. They appear on the dashboard, in `/api/status`, and in the event log when a warning or critical finding first appears.

All thresholds live in [`config.h`](../firmware/solar_tracker/config.h).

| Code | Severity | Rule | What to check |
|---|---|---|---|
| `INA219_MISSING` | Critical | INA219 did not answer at boot | SDA/SCL on GPIO 21/22, 3V3 to VCC |
| `TEMP_SENSOR` | Warning | DS18B20 returns −127 or out-of-range | Data on GPIO 4, 4.7 kΩ pull-up |
| `NIGHT` | Info | Average LDR < `DARK_THRESHOLD` (150) | Normal. Tracker walks back to face sunrise |
| `OPEN_CIRCUIT` | Critical | V > 19.5 V and I < 10 mA | Load side open: VIN−, load resistor, wire 13–14 |
| `NO_PANEL_VOLTAGE` | Critical | Bright light but V < 1 V | Fuse, panel + wire, wire 11–12 |
| `OVERHEAT` | Critical | Panel temperature > 60 °C | Airflow behind the panel |
| `TILT_STALL` | Critical | Commanding moves for 4 s but the LDR imbalance hasn't dropped by 20 % | Servo power, linkage, binding axle |
| `LDR_MISMATCH` | Warning | In bright light, one LDR < 15 % of the other for 10 s | LDR covered, dirty or disconnected |
| `AT_LIMIT` | Info | The sun is beyond the ±60° tilt range | Normal early and late in the day |
| `LDR_SATURATED` | Info | An LDR reads ≥ 4000 | Use a smaller divider resistor so brightness is measurable |
| `SOILING` | Warning | P < 80 % of the expected power for this light | Run a cleaning cycle (or let auto-clean do it) |
| `NOT_CALIBRATED` | Info | No clean-panel baseline saved yet | Clean the panel, then press **Calibrate** in full sun |
| `OK` | OK | No warning or critical findings | — |

## How the soiling check works

There is no pyranometer, so the LDRs act as the light reference. They sit outside the panel glass and don't collect the same dust.

1. **Calibrate** on a clean panel in direct sun. The firmware stores
   `k = P_clean / LDR_average`, corrected to 25 °C, in flash (survives reboots).
2. Every second it predicts `P_expected = k × LDR_average × (1 − 0.004 × (T − 25))`.
3. `soiling_ratio = P / P_expected`. Below 0.80 raises `SOILING`.
4. After every cleaning cycle the baseline is re-recorded automatically 10 s later, provided the light is good.

**Limitations.** LDRs are not linear and saturate in full sun with a 10 kΩ divider, so the ratio is a relative indicator, not an absolute efficiency figure. Calibrate at roughly the same time of day you expect to compare against. Re-recording after each clean also means a permanently damaged panel will not be flagged; that is a job for the TinyML phase.

## Auto-clean

When **Auto-clean** is on, a cycle starts if `SOILING` has been active for 1 minute (`AUTO_CLEAN_CONFIRM_MS`). At most one automatic cycle runs every 30 minutes (`AUTO_CLEAN_COOLDOWN_MS`). Tracking pauses while the blade is on the glass.

## Testing the rules

| To trigger | Do this |
|---|---|
| `LDR_MISMATCH` | Cover one LDR with tape in bright light for 10 s |
| `OPEN_CIRCUIT` | Disconnect the load resistor |
| `NO_PANEL_VOLTAGE` | Pull the fuse in daylight |
| `TILT_STALL` | Unplug the tilt servo's signal wire while the sun is off-axis |
| `SOILING` | Calibrate clean, then dust the glass with flour or talc |
| `OVERHEAT` | Warm the probe with a hair dryer |

The same faults can be tried without hardware in the [3D model](interactive/3d-model.html).
