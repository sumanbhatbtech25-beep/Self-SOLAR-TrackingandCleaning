# Bill of materials

Target budget for the single-ESP32 prototype with battery storage. Prices are estimates in Indian rupees (October 2026); compare suppliers and buy locally where you can to save on shipping.

| Qty | Component | Purpose | Target cost |
|---|---|---|---|
| 1 | ESP32 development board (DevKit V1, 30-pin) | Controller, Wi-Fi dashboard | ₹300–450 |
| 1 | INA219 current/voltage sensor module | Panel V, I, P | ₹90–150 |
| 2 | LDRs (5 mm) + 2 × 10 kΩ resistors + wiring | Sun direction sensing | ₹50–100 |
| 1 | DS18B20 waterproof probe + 4.7 kΩ resistor | Panel temperature | ₹100–160 |
| 1 | Solar panel, **10 W**, ~18 V | Charges the battery | ₹300–500 |
| 1 | Servo for panel tilt (MG90S preferred, SG90 works if balanced) | East–west tracking | ₹100–180 |
| 1 | Servo for wiper (SG90) | Cleaning | ₹100–180 |
| 1 | DC motor + TB6612FNG or L9110S driver *(optional, not fitted yet)* | Pump or linear wiper later | ₹150–250 |
| — | Breadboards (2), jumper wires, connectors | Prototype wiring | ₹150–250 |
| — | DIY frame: plywood base, aluminium uprights, axle, rails, wiper blade | Mechanics | ₹250–400 |
| — | Power supply and protection: LM2596 buck converter (3 A), 1 A inline fuse, 470–1000 µF capacitor, 100 kΩ + 27 kΩ + 0.1 µF for battery sensing | Power | ₹200–350 |
| 1 | 12 V 7 Ah sealed lead-acid (SLA) battery | Energy storage | ₹700–900 |
| 1 | PWM solar charge controller, 10 A, 12 V | Charging | ₹300–450 |
| — | 5 A inline fuse for the battery, 1 mm² wire | Battery protection | ₹100–150 |
| — | Miscellaneous and shipping | | ₹300–500 |
| | **Total as built (without DC motor)** | | **₹3,040–4,720** |
| | **Total with optional DC motor** | | **₹3,190–4,970** |

The battery system adds ₹1,100–1,500 to the original ₹2,090–3,470 plan.

## Buying notes

- **Solar panel:** with a battery, use the **10 W** panel; a 5 W panel cannot keep up with an ESP32 running Wi-Fi. Check that Voc is under 26 V (the INA219's limit). Typical "12 V" panels are 21–22 V.
- **Charge controller:** any 12 V PWM controller rated 10 A or more works. Common-positive and common-negative types both work with this wiring, because panel − goes only to the controller.
- **Battery:** a 12 V 7 Ah SLA is cheap, safe and forgiving. Avoid lithium cells unless you also buy a matching BMS and a lithium-capable controller.
- **Buck converter:** pick a module rated 3 A (LM2596 or similar). Two servos moving together can draw over 1 A.
- **Servos:** an MG90S (metal gears) holds the panel better against wind. An SG90 is fine if the axle runs through the panel's centre of mass.
- **INA219 module:** most come with a 0.1 Ω shunt, giving a ±3.2 A range. That is plenty for a 10 W panel (≈0.6 A).
- **Bench testing:** keep a 5 V 2 A phone-style adapter handy to power the electronics without the battery.

## Avoid for now

- Ready-made tracker mechanisms or slew drives
- MPPT controllers (worth it on bigger panels, not at 10 W)
- A second microcontroller

See the roadmap in the [README](../README.md#roadmap).
