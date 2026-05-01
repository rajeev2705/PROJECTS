/*----------------------------------------------------------------
 *
 * vskETarget.h
 *
 * Software to run the Air-Rifle / Small Bore Electronic Target
 * VSKTarget - ESP32-S3 Port (Rev 6.1)
 *
 * Based on freETarget v5.2 architecture:
 *   - ESP32-S3 PCNT hardware timers (no external logic chain)
 *   - A0505XT-1WR3-TR (isolated +/-5V for sensor boards)
 *   - FT231XS (USB-UART bridge)
 *   - Software VREF (no potentiometer)
 *   - Sensor filtering on separate daughter boards
 *
 *---------------------------------------------------------------*/
#ifndef _VSKETARGET_H_
#define _VSKETARGET_H_

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Preferences.h>
#include <USB.h>
#include <math.h>

#include "json.h"
#include "token.h"
#include "sensor_interface.h"

#define RESCUE     (1==0)

#if ( RESCUE )
#define SOFTWARE_VERSION "\"RESCUE ESP32-S3 April 2, 2026\""
#else
#define SOFTWARE_VERSION "\"6.1.0-ESP32S3 April 2, 2026\""
#endif

#define REV_100    100
#define REV_210    210
#define REV_220    220
#define REV_290    290
#define REV_300    300
#define REV_310    310
#define REV_320    320
#define REV_500    500   // ESP32-WROOM Version
#define REV_600    600   // ESP32-S3 Version (with external logic chain)
#define REV_610    610   // ESP32-S3 Simplified (PCNT, no external logic)

#define INIT_DONE       0xabcd
#define T(s)            (s)                       // ESP32-S3 unified memory

/*
 * Options
 */
#define SAMPLE_CALCULATIONS false

/*
 * Tracing
 */
#define DLT(level)      ( do_dlt(level) )
#define DLT_NONE          0
#define DLT_CRITICAL      0x80
#define DLT_APPLICATION   0x01
#define DLT_DIAG          0x02
#define DLT_INFO          0x04

/*
 * Serial Port Mapping - ESP32-S3
 * USB-CDC  = Native USB (GPIO19/GPIO20)
 * Serial0  = FT231XS bridge (GPIO43 TX / GPIO44 RX)
 * Serial1  = AUX / Token Ring (GPIO8 TX / GPIO0 RX)
 */
#define AUX_SERIAL         Serial1
#define DISPLAY_SERIAL     Serial2

/*
 * Timing via PCNT (Pulse Counter)
 * ESP32-S3 APB clock is 80MHz → 12.5ns resolution
 * Uses esp_timer_get_time() for microsecond timestamps
 * No external oscillator or counter IC needed
 */
#define CLOCK_MHZ         80.0                            // APB clock frequency
#define CLOCK_PERIOD      (1.0/CLOCK_MHZ)                 // Seconds per tick (12.5ns)
#define ONE_SECOND        1000L
#define ONE_SECOND_US     1000000u
#define FULL_SCALE        0xffffffff

#define SHOT_TIME     ((int)(json_sensor_dia / 0.33))
#define SHOT_STRING   20

#define HI(x) (((x) >> 8 ) & 0x00ff)
#define LO(x) ((x) & 0x00ff)
#define HHH10(x) (((x) / 10000 ) % 10)
#define HHI10(x) (((x) / 1000 ) % 10)
#define HLO10(x) (((x) / 100 ) % 10)
#define HI10(x)  (((x) / 10 ) % 10)
#define LO10(x)  ((x) % 10)

#define N       0
#define E       1
#define S       2
#define W       3
#define MISS    4

struct shot_r
{
  unsigned int shot_number;
  double       xphys_mm;
  double       yphys_mm;
  unsigned int timer_count[4];    // PCNT timestamps (microseconds)
  unsigned int face_strike;
  unsigned int sensor_status;
  unsigned long shot_time;
};

typedef struct shot_r shot_record_t;

struct GPIO {
  byte port;
  byte in_or_out;
  byte value;
};

typedef struct GPIO GPIO_t;
extern const GPIO init_table[];

extern double  s_of_sound;

extern const char* namesensor[];
extern const char to_hex[];
extern unsigned int face_strike;
extern const char nesw[];
extern shot_record_t record[];

/*----------------------------------------------------------------
 *
 * function: soft_reset
 *
 * brief:    Reset the board
 *
 * return:   Never
 *
 *----------------------------------------------------------------*/
static inline void soft_reset(void) { ESP.restart(); }

#endif /* _VSKETARGET_H_ */
