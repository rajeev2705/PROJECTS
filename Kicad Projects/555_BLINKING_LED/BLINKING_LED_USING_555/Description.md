# Police LED Flasher using 555 Timer

## Overview
This repository contains the design of a police LED flasher circuit using two 555 timer ICs. The circuit alternates flashing between red and blue LEDs, mimicking police vehicle lights.

## Features
- **Alternating Flashing:** Red and blue LEDs flash alternately.
- **Adjustable Timing:** Flash rate can be adjusted by changing resistor and capacitor values.
- **Simple Design:** Uses 555 timer ICs in astable mode to generate the flashing pattern.

## Components
- **7555xB:** Main timing ICs.
- **Red and Blue LEDs:** Flash alternately.
- **Resistors and Capacitors:** Set timing intervals.
- **Connectors:** For power supply.

## Folder Structure
- **/schematics:** Circuit diagrams and PCB layouts.
- **/bom:** Bill of materials listing all components.

## Schematic
The circuit has three sections:
- **PWM Generation:** U1 and U2 generate timing pulses.
- **LED Flash:** Red (D1, D2, D3) and Blue (D4, D5, D6) LEDs.
- **Battery:** Power supply connections.

## Usage
1. **Setup:** Assemble the circuit as per the schematic.
2. **Operation:** Connect 5V power supply and observe alternating flashing LEDs.
3. **Adjustments:** Change R and C values to adjust the flash rate.

## Flash Interval Calculation
The flash interval for the 555 timer in astable mode is given by:

- \( t_{high} = 0.693 \times (R1 + R2) \times C \)
- \( t_{low} = 0.693 \times R2 \times C \)

For example, with:
- R1 = 1MΩ
- R2 = 1MΩ
- C = 1μF

- \( t_{high} = 0.693 \times (1M + 1M) \times 1μF = 1.386 \text{ seconds} \)
- \( t_{low} = 0.693 \times 1M \times 1μF = 0.693 \text{ seconds} \)

Thus, the total period \( T = t_{high} + t_{low} \approx 2.079 \text{ seconds} \) and the frequency \( f = \frac{1}{T} \approx 0.481 \text{ Hz} \).

## Notes
- Ensure the power supply voltage is suitable for the components.
- Adjust resistor and capacitor values to achieve desired flash rates.
