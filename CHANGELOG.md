# Changelog

All notable changes to this project are recorded here.
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Planned
- Build the DIY frame, fit the battery system and bring the hardware up on the bench (see README roadmap)

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
