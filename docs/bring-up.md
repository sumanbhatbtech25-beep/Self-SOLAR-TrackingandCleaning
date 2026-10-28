# Bring-up and calibration

Build and test one block at a time. Each step has a small sketch in [`tests/`](../tests). Wire numbers refer to the [wire list](wiring.md#wire-list).

## Before you start

- Install the libraries listed in the [README](../README.md#2-install-the-libraries).
- Set the Serial Monitor to **115200 baud**.
- Have a multimeter ready.

## Steps

| Step | Wires | Test sketch | Pass when |
|---|---|---|---|
| 1. Rails | 1–4, 6 | — | 4.9–5.2 V on the 5 V rail, 3.2–3.4 V on the 3V3 rail, nothing else connected |
| 2. ESP32 | — | Any blink sketch | Board boots from the adapter |
| 3. INA219 | 7–10 | `tests/i2c_scan` | Finds a device at `0x40` |
| 4. DS18B20 | 22–25 | `tests/ds18b20_check` | Reads room temperature, rises when you hold the probe |
| 5. LDRs | 16–21 | `tests/ldr_check` | Covering one LDR drops its value sharply; sun reading stays below ~4000 |
| 6. Servos | 5, then 26–31 | `tests/servo_sweep` | Each servo moves smoothly; ESP32 does not reset |
| 7. Panel | 11–15 | `tests/ina219_check` | INA219 voltage matches a multimeter across the load within ~0.1 V |
| 8. Full firmware | — | `firmware/solar_tracker` | Dashboard loads, panel follows a torch moved east–west |

## Calibration

### Tilt centre and direction

1. Run `tests/servo_sweep` with `SERVO_PIN = 18`.
2. Type angles until the panel lies flat. Put that number in `TILT_SERVO_CENTER` in `config.h` (default 90).
3. Flash the main firmware and shine a torch on the **east** LDR. The panel should tip east, toward the torch. If it tips the other way, set `TILT_INVERT = true`.

### LDR range

In direct sun, both LDRs should read below about 4000. If they sit at 4095, replace both 10 kΩ divider resistors with 4.7 kΩ (or 2.2 kΩ). Use the same value on both sides.

If both LDRs read noticeably differently under the same light, swap them. If the gap persists, adjust `TRACK_DEADBAND` (default 0.04) until the panel stops hunting.

### Wiper sweep

1. Run `tests/servo_sweep` with `SERVO_PIN = 19`.
2. Find the angle where the blade rests just off the glass (`WIPER_PARK_DEG`) and the far end of the sweep (`WIPER_END_DEG`).
3. Make sure the blade touches the glass lightly along the whole arc. Too much pressure stalls an SG90.

### Clean-panel baseline

1. Clean the glass by hand.
2. Wait for a sunny moment with the tracker holding still.
3. Open the dashboard and press **Calibrate clean baseline**.

The baseline is saved in flash. After this the **Cleanliness** tile shows a percentage, and auto-clean can work.

## Logging data

The firmware prints one CSV line per second on Serial:

```
ms,voltage_v,current_a,power_w,panel_temp_c,ldr_east,ldr_west,tilt_deg,state,soiling_ratio,wiper
```

Capture it with the Arduino Serial Monitor, PlatformIO (`pio device monitor > log.csv`), or any serial terminal. Keep these logs: they are the training data for the TinyML phase.
