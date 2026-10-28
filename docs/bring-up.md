# Bring-up and calibration

Build and test one block at a time. Each step has a small sketch in [`tests/`](../tests). Wire numbers refer to the [wire list](wiring.md#wire-list).

## Before you start

- Install the libraries listed in the [README](../README.md#2-install-the-libraries).
- Set the Serial Monitor to **115200 baud**.
- Have a multimeter ready.
- Charge the battery before you start, or check it reads at least 12.4 V.

## Steps

| Step | Wires | Test sketch | Pass when |
|---|---|---|---|
| 1. Battery and buck | 1–7 | — | ESP32 **not** connected. Turn the buck's screw until the 5 V rail reads 5.0–5.2 V |
| 2. ESP32 | 8, 9, 11 | Any blink sketch | Board boots from the buck; 3V3 rail reads 3.2–3.4 V |
| 3. INA219 | 12–15 | `tests/i2c_scan` | Finds a device at `0x40` |
| 4. Battery sense | 20–22 | `tests/battery_check` | Printed battery voltage within ~0.1 V of a multimeter (see calibration) |
| 5. DS18B20 | 29–32 | `tests/ds18b20_check` | Reads room temperature, rises when you hold the probe |
| 6. LDRs | 23–28 | `tests/ldr_check` | Covering one LDR drops its value sharply; sun reading stays below ~4000 |
| 7. Servos | 10, then 33–38 | `tests/servo_sweep` | Each servo moves smoothly; ESP32 does not reset |
| 8. Panel | 16–19 | `tests/ina219_check` | In sunlight the controller's charging light is on and the INA219 shows current |
| 9. Full firmware | — | `firmware/solar_tracker` | Dashboard loads, battery tile shows a voltage, panel follows a torch moved east–west |

**Order matters for the charge controller:** connect the battery (wires 1–3) before the panel (wires 16–19), and disconnect the panel first. The controller decides it's a 12 V system from the battery voltage at power-up.

**Bench testing without a battery:** power the 5 V rail from a 5 V adapter instead of the buck, set `BATTERY_ENABLED = false` in `config.h`, and use a load resistor (≈33 Ω, 20 W) in place of the controller to give the INA219 something to measure.

## Calibration

### Battery voltage

1. Run `tests/battery_check` and measure the battery terminals with a multimeter at the same time.
2. Set `BATTERY_CAL` in `config.h` to `multimeter ÷ printed`. For example, 12.70 V ÷ 12.46 V = 1.02.

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
2. Wait for a sunny moment with the tracker holding still **and the battery below 13.6 V**. A full battery throttles charging, so the baseline would be wrong.
3. Open the dashboard and press **Calibrate clean baseline**.

The baseline is saved in flash and refreshed after every cleaning cycle.

## Logging data

The firmware prints one CSV line per second on Serial:

```
ms,voltage_v,current_a,power_w,panel_temp_c,battery_v,ldr_east,ldr_west,tilt_deg,state,soiling_ratio,wiper
```

Capture it with the Arduino Serial Monitor, PlatformIO (`pio device monitor > log.csv`), or any serial terminal. Keep these logs: they are the training data for the TinyML phase.
