# Changelog

All notable changes to this project are recorded here.
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Planned
- Build the DIY frame and bring the hardware up on the bench (see README roadmap)

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
