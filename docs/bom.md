# Bill of materials

Target budget for the single-ESP32 prototype. Prices are estimates in Indian rupees (October 2026); compare suppliers and buy locally where you can to save on shipping.

| Qty | Component | Purpose | Target cost |
|---|---|---|---|
| 1 | ESP32 development board (DevKit V1, 30-pin) | Controller, Wi-Fi dashboard | ₹300–450 |
| 1 | INA219 current/voltage sensor module | Panel V, I, P | ₹90–150 |
| 2 | LDRs (5 mm) + 2 × 10 kΩ resistors + wiring | Sun direction sensing | ₹50–100 |
| 1 | DS18B20 waterproof probe + 4.7 kΩ resistor | Panel temperature | ₹100–160 |
| 1 | Solar panel, 5–10 W, ~18 V | The panel under test | ₹300–500 |
| 1 | Servo for panel tilt (MG90S preferred, SG90 works if balanced) | East–west tracking | ₹100–180 |
| 1 | Servo for wiper (SG90) | Cleaning demonstration | ₹100–180 |
| 1 | DC motor + TB6612FNG or L9110S driver *(optional, not fitted yet)* | Pump or linear wiper later | ₹150–250 |
| — | Breadboards (2), jumper wires, connectors | Prototype wiring | ₹150–250 |
| — | DIY frame: plywood base, aluminium uprights, axle, rails, wiper blade | Mechanics | ₹250–400 |
| — | Power supply and protection: 5 V 2–3 A adapter, DC jack, 1 A inline fuse, 470–1000 µF capacitor, load resistor(s) | Power | ₹200–350 |
| — | Miscellaneous and shipping | | ₹300–500 |
| | **Total as built (without DC motor)** | | **₹1,940–3,220** |
| | **Total with optional DC motor** | | **₹2,090–3,470** |

## Buying notes

- **Servos:** an MG90S (metal gears) holds the panel better against wind. An SG90 is fine if the axle runs through the panel's centre of mass.
- **Solar panel:** check the label for Voc. It must stay under 26 V, the INA219's bus-voltage limit. Typical "12 V" 5–10 W panels have Voc ≈ 21–22 V.
- **INA219 module:** most come with a 0.1 Ω shunt, giving a ±3.2 A range. That is plenty for a 10 W panel (≈0.6 A).
- **Load resistor:** buy aluminium-cased ("gold") power resistors. They can be screwed to a heatsink.

## Avoid for now

- Ready-made tracker mechanisms or slew drives
- A large battery or charge controller
- A second microcontroller

Add these only once the core prototype works. See the roadmap in the [README](../README.md#roadmap).
