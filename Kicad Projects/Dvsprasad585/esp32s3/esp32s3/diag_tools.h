/*----------------------------------------------------------------
 *
 * diag_tools.h
 *
 * Debug and test tools
 * VSKTarget - ESP32-S3 Port (Rev 6.1)
 *
 *---------------------------------------------------------------*/
#ifndef _DIAG_TOOLS_H_
#define _DIAG_TOOLS_H_

#define T_HELP         0
#define T_DIGITAL      1
#define T_TRIGGER      2
#define T_CLOCK        3
#define T_OSCOPE       4
#define T_OSCOPE_PC    5
#define T_PAPER        6
#define T_SPIRAL       7
#define T_GRID         8
#define T_ONCE         9
#define T_PASS_THRU   10
#define T_SET_TRIP    11
#define T_XFR_LOOP    12
#define T_SERIAL_PORT 13
#define T_LED         14
#define T_FACE        15
#define T_WIFI        16
#define T_NONVOL      17
#define T_SHOT        18
#define T_WIFI_STATUS 19
#define T_WIFI_BROADCAST 20
#define T_LOG         21
#define T_SWITCH      25
#define T_S_OF_SOUND  26
#define T_TOKEN       27
#define T_LED_CYCLE   28
#define T_FORCE_CALC  29

/*
 * LED status messages
 */
#define LED_RESET         L('.', '.', '.')
#define LED_READY         L('*', '.', '.')
#define LED_OFF           L('.', '-', '-')
#define LED_TABATA_ON     L('-', '*', '-')
#define LED_TABATA_OFF    L('-', '.', '-')
#define LED_DONE          L('*', '*', '*')
#define LED_WIFI_SEND     L('.', '.', '*')
#define LED_HELLO         L('*', '.', '*')

#define NORTH_FAILED       L('.', '*', '.')
#define EAST_FAILED        L('.', '*', '*')
#define SOUTH_FAILED       L('*', '*', '*')
#define WEST_FAILED        L('*', '*', '.')

#define UNUSED_LED         L('*', '-', '*')

#define POST_COUNT_FAILED  0b001
#define VREF_OVER_UNDER    0b010
#define UNUSED_FAULT       0b100
#define SHOT_MISS          0b000

/*
 * Function Prototypes
 */
void self_test(uint16_t test);
void show_sensor_status(unsigned int sensor_status, shot_record_t* shot);
void blink_fault(unsigned int fault_code);
void POST_version(void);
void POST_LEDs(void);
bool POST_counters(void);
void POST_trip_point(void);
void set_trip_point(int v);
bool do_dlt(unsigned int level);

#endif
