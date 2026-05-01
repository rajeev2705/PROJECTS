/*----------------------------------------------------------------
 *
 * gpio.h
 *
 * Header file for GPIO functions
 * VSKTarget - ESP32-S3 Port (Rev 6.2)
 *
 * ESP32-S3-WROOM-1 Pin Mapping
 * Based on freETarget v5.2 architecture
 *
 * FULL SIGNAL CHAIN (Rev 6.2):
 *   - Filter network (10K + 10nF slope comp) on main board
 *   - 2x LM339 quad comparators on main board
 *   - 4x 74HC74 D flip-flop RUN latches on main board
 *   - ESP32-S3 ISR captures comparator output timestamps
 *   - PCNT-equivalent timing via esp_timer_get_time()
 *   - Sensor daughter boards: Piezo + OPA1641 only
 *   - A0505XT-1WR3-TR isolated ±5V for analog
 *
 * Signal Chain:
 *   Daughter Board: Piezo → OPA1641 amp → analog out via FRC
 *   Main Board: FRC → RC Filter → LM339 (vs VREF) → 74HC74 latch
 *               LM339 output also → ESP32-S3 GPIO ISR (timing)
 *               74HC74 Q → ESP32-S3 GPIO (RUN status)
 *
 *---------------------------------------------------------------*/
#ifndef _GPIO_H_
#define _GPIO_H_

#include "driver/pulse_cnt.h"

/*
 * Global functions
 */
void init_gpio(void);
void init_pcnt(void);
void arm_timers(void);
void clear_running(void);
unsigned int is_running(void);
void set_LED(int state_RDY, int state_X, int state_Y);
unsigned int read_DIP(unsigned int dip_mask);
void stop_timers(void);
void trip_timers(void);
bool read_in(unsigned int port);
void read_timers(unsigned int* timer_counts);
void drive_paper(void);
void enable_face_interrupt();
void disable_face_interrupt(void);
void enable_sensor_interrupt();
void disable_sensor_interrupt(void);
void multifunction_init(void);
void multifunction_switch(void);
void multifunction_display(void);
void multifunction_wait_open(void);
void output_to_all(char* s);
void char_to_all(char ch);
void digital_test(void);
void paper_on_off(bool on);
void rapid_green(unsigned int state);
void rapid_red(unsigned int state);
char get_all(void);
char aux_spool_read(void);
int  aux_spool_available(void);
void aux_spool_put(char ch);
char json_spool_read(void);
int  json_spool_available(void);
void json_spool_put(char ch);
void set_vref(unsigned int millivolts);

/*
 *  ESP32-S3 GPIO Pin Allocation (Rev 6.2 - Full Signal Chain)
 *  ==========================================================
 *
 *  Total available GPIOs: 45
 *  Total used: 25
 *  Spare: 20
 *
 *  MAIN BOARD SIGNAL CHAIN:
 *    FRC analog → RF1-RF4 (10K) + CF1-CF4 (10nF) → LM339 → 74HC74
 *    2x LM339 (quad comparator, SOIC-14) powered by +5V_ISO
 *    4x 74HC74 (dual D flip-flop, SOIC-14) powered by +3V3
 *    RP1-RP4 (10K pull-ups) on LM339 open-collector outputs
 *
 *  COMPARATOR OUTPUTS (LM339 → ESP32-S3 ISR for timing):
 *    COMP_N: GPIO4 (LM339 #1 OUT1) → ISR timestamp
 *    COMP_E: GPIO5 (LM339 #1 OUT2) → ISR timestamp
 *    COMP_S: GPIO6 (LM339 #2 OUT1) → ISR timestamp
 *    COMP_W: GPIO7 (LM339 #2 OUT2) → ISR timestamp
 *
 *  RUN LATCHES (74HC74 Q → ESP32-S3 for status polling):
 *    RUN_N: GPIO15 (74HC74 #1 Q1)
 *    RUN_E: GPIO16 (74HC74 #2 Q1)
 *    RUN_S: GPIO17 (74HC74 #3 Q1)
 *    RUN_W: GPIO18 (74HC74 #4 Q1)
 *
 *  LATCH CONTROL:
 *    LATCH_CLR: GPIO38 → 74HC74 /CLR (active low clear)
 *
 *  I2C PERIPHERALS (SDA=GPIO1, SCL=GPIO2):
 *    ADS1115 #1 (0x48): Sensor analog monitoring
 *    ADS1115 #2 (0x49): VREF/battery monitoring
 *    LM75 (0x4F):       Temperature sensor
 */

/*
 * Comparator Outputs (LM339 → ESP32-S3 GPIO)
 * These are the timing-critical inputs - ISR captures timestamp
 * LM339 open-collector with 10K pull-up to +3V3
 */
#define COMP_NORTH    4                    // LM339 #1 OUT1 → GPIO4
#define COMP_EAST     5                    // LM339 #1 OUT2 → GPIO5
#define COMP_SOUTH    6                    // LM339 #2 OUT1 → GPIO6
#define COMP_WEST     7                    // LM339 #2 OUT2 → GPIO7

/*
 * RUN Latch Outputs (74HC74 Q → ESP32-S3 GPIO)
 * Polled by is_running() to check which sensors have fired
 */
#define RUN_NORTH     15                   // 74HC74 #1 Q1 → GPIO15
#define RUN_EAST      16                   // 74HC74 #2 Q1 → GPIO16
#define RUN_SOUTH     17                   // 74HC74 #3 Q1 → GPIO17
#define RUN_WEST      18                   // 74HC74 #4 Q1 → GPIO18

/*
 * Latch Clear (ESP32-S3 → 74HC74 /CLR, active low)
 */
#define LATCH_CLR     38                   // Clear all RUN latches

/*
 * Legacy aliases for compatibility
 */
#define SENSOR_NORTH  COMP_NORTH
#define SENSOR_EAST   COMP_EAST
#define SENSOR_SOUTH  COMP_SOUTH
#define SENSOR_WEST   COMP_WEST

/*
 * LED Outputs (directly on ESP32-S3 GPIO)
 */
#define LED_RDY     47                    // Ready LED (green)
#define LED_X       48                    // X-axis LED (red)
#define LED_Y       45                    // Y-axis LED (yellow)

/*
 * Paper Motor (via 2N7002 MOSFET on GPIO)
 */
#define PAPER       46                    // Paper advance drive

/*
 * Face Sensor (interrupt capable)
 */
#define FACE_SENSOR 11                    // Face strike input

/*
 * Rapid Fire Outputs
 */
#define RAPID_RED_PIN    9                // Rapid fire RED output
#define RAPID_GREEN_PIN 10                // Rapid fire GREEN output

/*
 * Software-Controlled VREF
 * ESP32-S3 LEDC PWM → R7 (10K) + C9 (100nF) RC filter
 * Sent to LM339 inverting inputs as comparator threshold
 * Also sent to daughter boards via FRC pin 9
 */
#define VREF_DAC_PIN  3                   // PWM output for VREF control
#define VREF_CHANNEL  0                   // LEDC channel

/*
 * DIP Switches - controlled via JSON (no physical switches)
 */
#define CALIBRATE       (0)
#define DIP_SW_A        (0)
#define CAL_LOW         (DIP_SW_A)
#define DIP_SW_B        (0)
#define CAL_HIGH        (DIP_SW_B)
#define VERBOSE_TRACE   (0)

/*
 * PWM for LED illumination (using ESP32-S3 LEDC peripheral)
 */
#define LED_PWM_PIN      14               // Spare GPIO for LED strip PWM
#define VSET_PWM_CHANNEL  0               // LEDC channel (shared with VREF)
#define LED_PWM_CHANNEL   1               // LEDC channel
#define PWM_FREQUENCY     5000            // 5 kHz PWM
#define PWM_RESOLUTION    8               // 8-bit resolution (0-255)

#define LON          1
#define LOF          0
#define LXX         -1
#define L(A, B, C)  (A), (B), (C)

#define NORTH        0
#define EAST         1
#define SOUTH        2
#define WEST         3
#define TRIP_NORTH   0x01
#define TRIP_EAST    0x02
#define TRIP_SOUTH   0x04
#define TRIP_WEST    0x08

/*
 * Paper Motor
 */
#define PAPER_ON      1
#define PAPER_OFF     0

/*
 * Serial Port Pins for ESP32-S3
 * USB-CDC  = Native USB (GPIO19/GPIO20) - primary serial
 * UART0    = FT231XS bridge (GPIO43 TX / GPIO44 RX)
 * Serial1  = AUX/Token Ring (GPIO8 TX / GPIO0 RX)
 */
#define AUX_TX_PIN    8
#define AUX_RX_PIN    0

#define DISP_TX_PIN  43                   // UART0 TX (FT231XS)
#define DISP_RX_PIN  44                   // UART0 RX (FT231XS)

/*
 * I2C Bus (DEDICATED - no pin sharing)
 */
#define I2C_SDA       1
#define I2C_SCL       2

/*
 * USB Native (ESP32-S3 USB-OTG)
 */
#define USB_DM       19
#define USB_DP       20

/*
 * PCNT Configuration (timing via ISR + esp_timer)
 * Resolution: 1 microsecond via esp_timer_get_time()
 */
#define PCNT_UNIT_NORTH  0
#define PCNT_UNIT_EAST   1
#define PCNT_UNIT_SOUTH  2
#define PCNT_UNIT_WEST   3

/*
 * Multifunction Switch Use (same as original)
 */
#define HOLD1(x)    LO10((x))
#define HOLD2(x)    HI10((x))
#define TAP1(x)     HLO10((x))
#define TAP2(x)     HHI10((x))
#define HOLD12(x)   HHH10((x))

#define POWER_TAP     0
#define PAPER_FEED    1
#define LED_ADJUST    2
#define PAPER_SHOT    3
#define PC_TEST       4
#define ON_OFF        5
#define MFS_SPARE_6   6
#define MFS_SPARE_7   7
#define MFS_SPARE_8   8
#define TARGET_TYPE   9

#define NO_ACTION     0
#define RAPID_RED     1
#define RAPID_GREEN   2

#define RED_OUT      RAPID_RED_PIN
#define GREEN_OUT    RAPID_GREEN_PIN
#define RED_MASK     1
#define GREEN_MASK   8

#define EOF_MARKER 0xFF

#endif
