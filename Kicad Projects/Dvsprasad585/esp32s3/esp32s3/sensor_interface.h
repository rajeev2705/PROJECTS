/*----------------------------------------------------------------
 *
 * sensor_interface.h
 *
 * Sensor Board Interface Definition
 * VSKTarget - ESP32-S3 Port (Rev 6.2)
 *
 * Based on freETarget v5.2 architecture
 *
 * DESIGN (Rev 6.2):
 *   - Sensor daughter boards: Piezo + OPA1641 amplifier only
 *   - Main board has full signal conditioning chain:
 *     RC Filter (10K+10nF) → LM339 comparator → 74HC74 latch
 *   - A0505XT-1WR3-TR provides isolated ±5V for OPA1641
 *   - FRC ribbon cable carries analog signals and power
 *   - ESP32-S3 ISR captures LM339 output timestamps (1us)
 *   - 74HC74 latches provide polled RUN status
 *
 *---------------------------------------------------------------*/
#ifndef _SENSOR_INTERFACE_H_
#define _SENSOR_INTERFACE_H_

/*
 *  Sensor Daughter Board Circuit (identical for all 4 boards)
 *  ===========================================================
 *  Each sensor board is a small PCB mounted beside/near the sensor.
 *  It contains ONLY the piezo and pre-amplifier.
 *  Signal conditioning (filter, comparator, latch) is on main board.
 *
 *  MK1 (Piezo Microphone)
 *    Pin 1 → R_bias (2K2) → +5V_ISO bias
 *    Pin 2 → C_ac (0.1uF) AC coupling → OPA1641 input
 *
 *  U1 (OPA1641 Op-Amp)
 *    Pin 2 (IN-)  ← C_ac/R_in input, R_fb (15K) feedback from output
 *    Pin 3 (IN+)  ← R_bias/MK1 junction (bias point)
 *    Pin 4 (V-)   ← -5V_ISO (from A0505XT on main board, via FRC)
 *    Pin 6 (OUT)  → FRC signal pin (raw amplified analog)
 *    Pin 7 (V+)   ← +5V_ISO (from A0505XT on main board, via FRC)
 *
 *  Output: Raw amplified analog signal sent to main board via FRC
 *  Main board does all filtering and comparison.
 */

/*
 *  Main Board Signal Conditioning (Rev 6.2)
 *  ==========================================
 *
 *  For each sensor channel (N, E, S, W):
 *
 *  1. RC SLOPE COMPENSATION FILTER:
 *     FRC analog → RF[n] (10K) → junction → to LM339 IN+
 *                                   ↓
 *                              CF[n] (10nF) → GND
 *     Cutoff: fc = 1/(2π × 10K × 10nF) = 1.59 kHz
 *     This matches the Filter_Network.pdf specification
 *
 *  2. LM339 COMPARATOR:
 *     IN+ ← Filtered sensor signal (from RC filter)
 *     IN- ← VREF_FILT (software-controlled threshold)
 *     OUT → Open-collector, pulled up to +3V3 via RP[n] (10K)
 *     Idle: HIGH (pull-up). Trigger: LOW (open-collector pulls down)
 *     Power: VCC = +5V_ISO, GND
 *
 *  3. 74HC74 D FLIP-FLOP LATCH:
 *     CLK ← LM339 output (triggers on falling edge)
 *     D   ← +3V3 (always high, so Q goes HIGH on clock)
 *     Q   → ESP32-S3 RUN_[N/E/S/W] GPIO (polled by is_running)
 *     /CLR ← LATCH_CLR (GPIO38, active low, clears all latches)
 *     /PR  ← +3V3 (preset inactive)
 *     Power: VCC = +3V3, GND
 */

/*
 *  10-Pin FRC Connector (J2 on main board)
 *  ========================================
 *  Carries ANALOG signals from daughter boards (NOT digital!)
 *
 *     Left Column       Right Column
 *    +---------------------------------+
 *    |  Pin 1 (NORTH analog) Pin 2 (+5V_ISO)  |
 *    |  Pin 3 (EAST analog)  Pin 4 (+5V_ISO)  |
 *    |  Pin 5 (SOUTH analog) Pin 6 (GND)      |
 *    |  Pin 7 (WEST analog)  Pin 8 (-5V_ISO)  |
 *    |  Pin 9 (VREF out)     Pin 10(GND)      |
 *    +---------------------------------+
 *
 *  Signal pins carry ANALOG voltages (OPA1641 output).
 *  All digital processing happens on the main board.
 */

/*
 *  FRC Signal Pin Assignments (analog from daughter boards)
 */
#define FRC_NORTH_ANALOG  1
#define FRC_EAST_ANALOG   3
#define FRC_SOUTH_ANALOG  5
#define FRC_WEST_ANALOG   7
#define FRC_VREF_OUT      9     // VREF from main board to daughter boards

/*
 *  FRC Power Pin Assignments
 */
#define FRC_VCC_ISO_1     2     // +5V isolated (to daughter board OPA1641 V+)
#define FRC_VCC_ISO_2     4     // +5V isolated (redundant)
#define FRC_GND_1         6     // GND
#define FRC_NEG5V_ISO     8     // -5V isolated (to daughter board OPA1641 V-)
#define FRC_GND_2        10     // GND

/*
 *  Slope Compensation Filter (on main board)
 *  Matches Filter_Network.pdf specification
 */
#define SLOPE_R_VALUE     10000   // 10K ohms (RF1-RF4)
#define SLOPE_C_VALUE     10      // 10 nF (CF1-CF4) = 0.01uF
#define SLOPE_FC_HZ       1592   // Cutoff: 1/(2pi*10K*10nF) ≈ 1.59 kHz

/*
 *  Main Board Signal Routing (Rev 6.2)
 *  =====================================
 *  FRC Pin 1 (North) → RF1 (10K) → CF1 (10nF) → LM339#1 IN1+
 *  FRC Pin 3 (East)  → RF2 (10K) → CF2 (10nF) → LM339#1 IN2+
 *  FRC Pin 5 (South) → RF3 (10K) → CF3 (10nF) → LM339#2 IN1+
 *  FRC Pin 7 (West)  → RF4 (10K) → CF4 (10nF) → LM339#2 IN2+
 *
 *  LM339#1 OUT1 → RP1 (10K PU) → GPIO4 (COMP_N ISR) + 74HC74#1 CLK
 *  LM339#1 OUT2 → RP2 (10K PU) → GPIO5 (COMP_E ISR) + 74HC74#2 CLK
 *  LM339#2 OUT1 → RP3 (10K PU) → GPIO6 (COMP_S ISR) + 74HC74#3 CLK
 *  LM339#2 OUT2 → RP4 (10K PU) → GPIO7 (COMP_W ISR) + 74HC74#4 CLK
 *
 *  74HC74#1 Q1 → GPIO15 (RUN_N)
 *  74HC74#2 Q1 → GPIO16 (RUN_E)
 *  74HC74#3 Q1 → GPIO17 (RUN_S)
 *  74HC74#4 Q1 → GPIO18 (RUN_W)
 *
 *  GPIO38 → 74HC74 /CLR (all 4 chips, active low)
 *
 *  VREF: GPIO3 (PWM) → R7 (10K) + C9 (100nF) → LM339 IN- (all 4)
 *        Also → FRC Pin 9 (to daughter boards if needed)
 */

/*
 *  ADS1115 External ADC Configuration
 *  I2C Address: 0x48 (ADDR->GND) for sensors, 0x49 (ADDR->VDD) for references
 *  I2C Bus: SDA=GPIO1, SCL=GPIO2 (DEDICATED)
 *
 *  Note: ADS1115 reads analog levels from FRC for diagnostics only.
 *  Shot timing is handled by LM339 comparator + ESP32-S3 ISR.
 */
#define ADS1115_SENSOR_ADDR   0x48
#define ADS1115_REF_ADDR      0x49

#define ADS_NORTH_CHANNEL     0
#define ADS_EAST_CHANNEL      1
#define ADS_SOUTH_CHANNEL     2
#define ADS_WEST_CHANNEL      3

#define ADS_VREF_CHANNEL      0
#define ADS_V12_CHANNEL       1

/*
 *  Component Summary (Rev 6.2 Main Board)
 *  ========================================
 *  ICs (13 total):
 *    U1:  ESP32-S3-WROOM-1 (MCU)
 *    U2:  FT231XS (USB-UART bridge)
 *    U3:  A0505XT-1WR3-TR (isolated ±5V DC-DC)
 *    U4:  AMS1117-3.3 (3.3V LDO regulator)
 *    U5:  ADS1115 (0x48, sensor ADC)
 *    U6:  ADS1115 (0x49, reference ADC)
 *    U7:  LM75 (0x4F, temperature sensor)
 *    U8:  LM339 #1 (quad comparator, N/E channels)
 *    U9:  LM339 #2 (quad comparator, S/W channels)
 *    U10: 74HC74 #1 (RUN latch North)
 *    U11: 74HC74 #2 (RUN latch East)
 *    U12: 74HC74 #3 (RUN latch South)
 *    U13: 74HC74 #4 (RUN latch West)
 *
 *  Discrete:
 *    Q1:  2N7002 (paper motor MOSFET)
 *    D1:  1N4148W (motor flyback diode)
 *    D2:  PRTR5V0U2X (USB ESD protection)
 *    D3:  Green LED (power indicator)
 *    D4:  Green LED (RDY status)
 *    D5:  Red LED (X status)
 *    D6:  Yellow LED (Y status)
 *
 *  Filter Network (slope compensation):
 *    RF1-RF4: 10K ohm (0603)
 *    CF1-CF4: 10nF (0603)
 *
 *  Pull-ups (LM339 open-collector):
 *    RP1-RP4: 10K ohm (0603) to +3V3
 *
 *  Other Resistors:
 *    R1-R2: 4.7K (I2C pull-ups)
 *    R3: 1K (motor gate)
 *    R4-R6: 330 ohm (LED limiters)
 *    R7: 10K (VREF RC filter)
 *    R8: 1K (power LED)
 *    R9: 10K (EN pull-up)
 *
 *  Capacitors (19 total):
 *    C1: 100nF (ESP32 decoupling)
 *    C2: 10uF (3V3 bulk)
 *    C3: 100nF (LDO input)
 *    C4: 22uF (LDO output)
 *    C5: 100nF (FT231XS)
 *    C6-C8: 100nF (ADS1115 x2, LM75)
 *    C9: 100nF (VREF filter)
 *    C10: 100nF (DC-DC input)
 *    C11: 100nF (DC-DC output)
 *    C12: 10uF (5V bulk)
 *    C13-C14: 100nF (LM339 #1, #2)
 *    C15-C18: 100nF (74HC74 #1-#4)
 *    C19: 4.7uF (USB)
 *
 *  Connectors:
 *    J1: USB-C
 *    J2: FRC 10-pin (2x5, sensor interface)
 *    J3: Motor/LED 8-pin
 *    J4: AUX serial 4-pin
 *    J5: Rapid fire 4-pin
 *    J6: Face sensor 3-pin
 *    J7-J8: Multifunction switches 2-pin
 *
 *  Switches: SW1 (Reset), SW2 (Boot/GPIO0)
 *  Mounting Holes: H1-H4 (M3)
 *  Board: 160 x 100 mm, 2-layer
 */

/*
 *  Power Supply (A0505XT-1WR3-TR on main board)
 *  ==============================================
 *  Input:  +5V (from USB or DC barrel jack)
 *  Output: +5V_ISO → FRC pins 2,4 → daughter board OPA1641 V+
 *          -5V_ISO → FRC pin 8    → daughter board OPA1641 V-
 *          +5V_ISO → U8,U9 LM339 VCC (analog comparator power)
 */

#endif /* _SENSOR_INTERFACE_H_ */
