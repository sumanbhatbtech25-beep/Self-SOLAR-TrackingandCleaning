# Wiring

![Wiring schematic](images/wiring-schematic.svg)

For an interactive version with a tick-off wire list, open [`interactive/wiring-sheet.html`](interactive/wiring-sheet.html) in a browser.

## Power plan

| Rail | Source | Feeds |
|---|---|---|
| **5 V** | 5 V 2–3 A adapter → DC jack → breadboard rail | ESP32 VIN, both servos, optional motor driver |
| **3.3 V** | ESP32 3V3 pin → second breadboard rail | INA219, LDR dividers, DS18B20 |
| **Panel** | Solar panel → 1 A fuse → INA219 → load resistor | Nothing else. Measurement only |
| **GND** | One common ground | Everything, **including panel −** |

If your adapter is 9–12 V, add a buck converter after the jack and set it to 5.1 V with a multimeter before connecting anything.

## ESP32 pin map

| ESP32 pin | Connects to | Notes |
|---|---|---|
| VIN | 5 V rail | Unplug while uploading over USB unless your board has a USB protection diode |
| GND | GND rail | Either GND pin |
| 3V3 | 3.3 V rail | Sensors only, never servos |
| GPIO 4 | DS18B20 data | 4.7 kΩ pull-up to 3.3 V |
| GPIO 18 | Tilt servo signal | 50 Hz PWM |
| GPIO 19 | Wiper servo signal | |
| GPIO 21 | INA219 SDA | |
| GPIO 22 | INA219 SCL | |
| GPIO 34 | East LDR divider | ADC1 (keeps working with Wi-Fi on) |
| GPIO 35 | West LDR divider | ADC1 |
| GPIO 25 / 26 / 27 | Motor driver PWMA / AIN1 / AIN2 | Optional |

Avoid GPIO 6–11 (flash) and GPIO 0, 2, 5, 12, 15 (boot strapping pins). Don't put analog sensors on ADC2 pins (GPIO 0, 2, 4, 12–15, 25–27); ADC2 stops working once Wi-Fi starts.

## Wire list

Wire in this order. Numbers match the [bring-up guide](bring-up.md).

### Power
| # | From | To | Notes |
|---|---|---|---|
| 1 | Adapter + | 5 V rail | Through the DC jack |
| 2 | Adapter − | GND rail | |
| 3 | 5 V rail | ESP32 VIN | |
| 4 | GND rail | ESP32 GND | |
| 5 | C1 + / − | 5 V rail / GND rail | 470–1000 µF, ≥10 V. Stripe side is −. Fit near the servo connectors |
| 6 | ESP32 3V3 | 3.3 V rail | |

### INA219
| # | From | To | Notes |
|---|---|---|---|
| 7 | INA219 VCC | 3.3 V rail | |
| 8 | INA219 GND | GND rail | |
| 9 | INA219 SDA | GPIO 21 | |
| 10 | INA219 SCL | GPIO 22 | |

### Panel circuit (use 0.5 mm² or thicker wire)
| # | From | To | Notes |
|---|---|---|---|
| 11 | Panel + | Fuse F1 (1 A) | Inline fuse holder |
| 12 | Fuse F1 | INA219 VIN+ | Screw terminal |
| 13 | INA219 VIN− | Load resistor | Screw terminal |
| 14 | Load resistor | Panel − | See load sizing below |
| 15 | Panel − | GND rail | **Required.** The INA219 measures bus voltage from VIN− to GND |

### LDR pair
| # | From | To | Notes |
|---|---|---|---|
| 16 | East LDR, leg 1 | 3.3 V rail | |
| 17 | East LDR, leg 2 | GPIO 34 | Junction with the 10 kΩ |
| 18 | 10 kΩ resistor | GPIO 34 → GND | Optional 0.1 µF from GPIO 34 to GND |
| 19 | West LDR, leg 1 | 3.3 V rail | |
| 20 | West LDR, leg 2 | GPIO 35 | Junction with the 10 kΩ |
| 21 | 10 kΩ resistor | GPIO 35 → GND | |

Mount the two LDRs side by side with a black divider wall between them, about 3 cm tall, running north–south. When the sun is off to one side the wall shades one LDR.

### DS18B20
| # | From | To | Notes |
|---|---|---|---|
| 22 | Red (VDD) | 3.3 V rail | |
| 23 | Black (GND) | GND rail | |
| 24 | Yellow (DQ) | GPIO 4 | Some probes use white or blue for data |
| 25 | 4.7 kΩ resistor | GPIO 4 → 3.3 V | Pull-up |

Tape the probe to the back of the panel with aluminium tape.

### Servos
| # | From | To | Notes |
|---|---|---|---|
| 26 | Tilt servo brown | GND rail | |
| 27 | Tilt servo red | 5 V rail | |
| 28 | Tilt servo orange | GPIO 18 | |
| 29 | Wiper servo brown | GND rail | |
| 30 | Wiper servo red | 5 V rail | |
| 31 | Wiper servo orange | GPIO 19 | |

### Optional motor driver (TB6612FNG)
| # | From | To |
|---|---|---|
| 32 | VM | 5 V rail |
| 33 | VCC | 3.3 V rail |
| 34 | GND | GND rail |
| 35 | STBY | 3.3 V rail |
| 36 | PWMA | GPIO 25 |
| 37 | AIN1 | GPIO 26 |
| 38 | AIN2 | GPIO 27 |
| 39 | A01 / A02 | DC motor |

## Load resistor sizing

The panel's output goes into a resistor so the INA219 has a real current to measure.

| Panel | Load | Rating | How |
|---|---|---|---|
| 10 W, ~18 V Vmp | ≈ 33 Ω | 20 W | Two 68 Ω 10 W resistors in parallel |
| 5 W, ~18 V Vmp | ≈ 68 Ω | 10 W | One 68 Ω 10 W resistor |

The resistor dissipates nearly the full panel output and gets hot. Mount it on a piece of aluminium, off the breadboard. A 12 V LED strip or a small 12 V fan also works as a load.

## Common problems

| Symptom | Likely cause |
|---|---|
| Voltage reads 0 but current is fine | Panel − not connected to GND (wire 15) |
| ESP32 resets when a servo moves | Missing C1, or servo powered from 3V3 |
| LDRs read 4095 in sun | Divider resistor too large. Try 4.7 kΩ or 2.2 kΩ |
| DS18B20 reads −127 | Missing 4.7 kΩ pull-up or wrong data wire |
| Panel turns away from the sun | Set `TILT_INVERT = true` in `config.h` |
| Wires pulling at the axle | Leave a slack loop for ±60° of travel |
