# Changelog

All notable changes to this project are recorded here.
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Planned
- Build the DIY frame, fit the battery system and bring the hardware up on the bench (see README roadmap)

## [0.5.0] - 2026-11-10

### Changed
- Firmware rewritten for the as-built demo rig: 12 V 2 A adapter, 8 panels (4S2P) into a 4.4 Ω test load, INA219 + ACS712 in series, 4 LDR modules as two pairs (GPIO 34/35), photo transistor light reference (GPIO 39), DS18B20 (GPIO 4), DHT11 (GPIO 23), relays on GPIO 25/26, tilt and wiper MG996R servos on GPIO 18/19
- Wiper is now a foam flap on a rod: sweeps the left row, then the right row, and parks straight up
- Dust check is light-referenced: Rule B = I / (k·L) raises `SOILING`; Rule A = I / I₀ is logged for comparison. Cleaning no longer re-calibrates automatically
- `DEMO_MODE`: real-time torch tracking and short check intervals; normal mode uses energy-saving intervals and powers the servos only while moving

### Added
- Relay self-test (zero check and ACS712 re-zero with the panels disconnected, then sensor agreement and V = I·R on the load) and ACS712 zeroing at boot
- 18 fault rules, including `CURRENT_SENSOR_MISMATCH` with light-referenced arbitration, `PANEL_DISCONNECTED`, `NO_CURRENT`, `TILT_NO_RESPONSE`, `PANEL_HOT_VS_AIR`, `SUPPLY_LOW/HIGH`
- 1-second CSV log with experiment `label` and one-shot `mark` columns; Serial and dashboard commands
- `tests/calibration` sketch, `experiments/PROTOCOL.md` (E1–E6), `analysis/` (capture, analysis with figures and LaTeX tables, synthetic pipeline test)

### Removed
- Battery, charge-controller and deep-sleep code (the demo runs from an adapter); `tests/battery_check`

## [0.2.0] - 2026-10-29

### Added
- Battery storage: 12 V 7 Ah SLA battery, 10 A PWM charge controller, 5 A battery fuse and a 12→5 V buck converter
- `power` module: battery voltage on GPIO 32 (100 kΩ / 27 kΩ divider), low-battery cut-off at 11.8 V with resume at 12.3 V, night deep sleep in 15-minute naps, tilt position kept across sleep
- Battery diagnostics: `BATTERY_HIGH`, `BATTERY_CRITICAL`, `BATTERY_LOW`, `BATTERY_FULL`, `BATTERY_SENSOR`, plus `NO_CHARGE` (18 rules in total)
- Battery tile and night-sleep note on the dashboard; battery fields in `/api/status`; `battery_v` column in the CSV log
- `tests/battery_check` sketch and a battery calibration step (`BATTERY_CAL`)
- 3D model: battery and charge controller on a longer base, 24-hour day/night play, battery charge, energy flow and sleep simulation
- Energy budget in `docs/wiring.md`

### Changed
- Panel now charges the battery through the controller instead of heating a load resistor
- Panel − connects only to the controller's PV− terminal (no longer to ground)
- Soiling check now compares panel **current** instead of power, because a PWM controller holds the panel near battery voltage. The saved baseline key changed, so recalibrate once after updating
- Charging-path and soiling checks pause while the battery is full
- Wire list renumbered (46 wires); bring-up order starts with the battery and buck converter
- Budget: ₹3,040–4,720 without the optional DC motor

### Removed
- 5 V wall adapter as the main supply (still supported for bench tests with `BATTERY_ENABLED = false`)
- Load resistor from the main build (still useful for bench tests)

## [0.1.0] - 2026-10-29

### Added
- System design for a single-ESP32 prototype with a ₹2,090–3,470 budget
- Interactive 3D model of the rig with tracking, cleaning and fault simulation (`docs/interactive/3d-model.html`)
- Complete wiring: schematic, pin map, 39-wire list, load sizing (`docs/wiring.md`, `docs/interactive/wiring-sheet.html`)
- Firmware v0.1:
  - LDR-based single-axis tracking with ±60° limits, night park and stall detection
  - Non-blocking servo wiper cycle, manual and automatic
  - INA219 power and DS18B20 temperature monitoring
  - 12 rule-based diagnostics with a flash-stored clean-panel baseline
  - Wi-Fi dashboard and JSON API, mDNS, access-point fallback
  - CSV logging over Serial
- Bring-up test sketches for I²C, INA219, DS18B20, LDRs and servos
- Documentation: BOM, bring-up and calibration guide, diagnostics reference
