# Experiment protocol (E1–E6)

How to collect the data for the paper with firmware v0.5, and how to turn it into results with `analysis/analyze.py`. Every number in the paper must come from logs you record on the rig.

## Before every session

1. Flash `firmware/solar_tracker` with your calibration values in `config.h` (from `tests/calibration`).
2. Log with `python analysis/capture.py --port <COM port> data/raw/<file>.csv` (close the Arduino Serial Monitor first). You can also copy the Serial Monitor text into a file; lines starting with `#` are ignored.
3. Leave **auto-clean off** (the default) except where a step says otherwise.
4. Fill one row of the notes sheet at the end of this file for each session.

## Commands you will use

| Command | Effect |
|---|---|
| `label E2_light_s1` | Tags every following CSV line. Format: `<experiment>_<condition>_<session>` |
| `mark inject_ldr` | Tags the next CSV line once (fault injections, shade passes) |
| `calibrate` | Saves the clean-panel baseline (clean panel, steady light) |
| `clean` | Runs one wiper cycle |
| `mode auto` / `mode fixed 0` | Tracking on / held flat (E6) |
| `selftest` | Relay self-test of both current sensors |

The same commands are on the dashboard (label and mark boxes at the bottom).

## Light source

Use **steady light**: sunlight on a clear day, or a lamp fixed in place for indoor runs. A hand-held torch is fine for the tracking demo, but its light varies too much for E1–E4. Record which light you used.

## E1 Baseline noise (1–2 sessions)

1. Clean the panels by hand. Light steady.
2. `calibrate`, wait 30 s.
3. `label E1_clean_s1` and log **10 minutes** without touching anything.
4. Repeat on another day as `E1_clean_s2`.

## E2 Dust levels (2 sessions)

Cover **all 8 panels equally**. Your panels are 4 in series per string, so dust on one panel cuts the whole string and gives a misleading curve.

| Level | Tracing paper | Or talc |
|---|---|---|
| clean | none | none |
| light | 1 layer | 0.5 g spread over the whole array |
| medium | 2 layers | 1.0 g |
| heavy | 3 layers | 2.0 g |

1. Clean panels, `calibrate`.
2. For each level: apply it, `label E2_<level>_s1`, log **10 minutes**.
3. Repeat the whole set as `_s2`.

The photo transistor must stay **uncovered**: it is the light reference.

## E3 Cloud test (10 passes per session)

1. Clean panels, calibrated. `label E3_cloud_s1`.
2. Every 30 s, pass a **card** over the panels **and the photo transistor together**, holding it there about 5 s. Type `mark cloud` as each pass starts.
3. 10 passes per session; repeat as `_s2`, `_s3`.

Result: Rule A (current only) should trigger on most passes; Rule B (light-referenced) should not.

## E4 Cleaning (5 runs per level)

Use **talc**, since the foam can't wipe tracing paper.

1. Apply the level, `label E4_<level>_s<run>` (e.g. `E4_medium_s3`).
2. Wait **60 s**, then `clean`. Leave everything untouched for **80 s** after the wiper parks.
3. Re-apply dust for the next run. 5 runs each for light, medium, heavy.

## E5 Fault injection (5 trials per fault)

`label E5_faults_s1`. For each trial: wait 40 s, type the mark **as you inject the fault**, wait up to 60 s for the alert, undo the fault, type `mark restore_<fault>`.

| Mark | Do this | Expected alert |
|---|---|---|
| `mark inject_ldr` | Cover **one LDR pair** (both modules on one edge) with tape, light on | `LDR_MISMATCH` |
| `mark inject_panel` | Unplug the panels' + wire at the ACS712 | `PANEL_DISCONNECTED` / `NO_CURRENT` |
| `mark inject_load` | Unplug the 4.4 Ω load from relay NC1 | `NO_CURRENT` |
| `mark inject_servo` | Unplug the tilt servo's orange wire, then shine the light from one side | `TILT_NO_RESPONSE` |
| `mark inject_temp` | Warm the DS18B20 with a hair dryer | `PANEL_HOT_VS_AIR` |
| `mark inject_ina` | Unplug the INA219's SDA wire | `INA219_MISSING` |
| `mark inject_acs` | Unplug the ACS712 OUT wire from the divider | `ACS712_FAULT` |

Result: detection rate and seconds from injection to alert.

## E6 Tracking vs fixed (3–4 block pairs)

Outdoors in sunlight, same time window on each day. Alternate **15–20 minute** blocks:

1. `mode fixed 0`, `label E6_fixed_b1`, log one block.
2. `mode auto`, `label E6_track_b1`, log one block.
3. Repeat as `b2`, `b3`, `b4` (swap the order on alternate pairs: track first, then fixed).

Raw panel current is compared. Don't normalise by the LDRs or the photo transistor: they tilt with the panel, so dividing by them would cancel the tracking gain.

## Analysis

```bash
pip install -r analysis/requirements.txt
python analysis/analyze.py data/raw          # writes analysis/output/
```

It cleans the data (drops −127 °C rows, sensor errors, the first 30 s after each change or mark, wiper and self-test rows), then writes `summary.md`, a CSV and a LaTeX table per experiment, and four figures (PNG + PDF):

1. Soiling ratio vs dust level (E2)
2. Cloud test: current, light and both rules over time (E3), the main figure
3. Before and after cleaning (E4)
4. Tracking vs fixed (E6)

Fault detection (E5) is a table.

`python analysis/analyze.py --synthetic` runs the whole pipeline on generated data so you can check it works. Those outputs are watermarked **SYNTHETIC** and must never be used in the paper.

## Notes sheet

| Date | File | Labels | Light source | Dust method | What you did / anything unusual |
|---|---|---|---|---|---|
| | | | | | |
