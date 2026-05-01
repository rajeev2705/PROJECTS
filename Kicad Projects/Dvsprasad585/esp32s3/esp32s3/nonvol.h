/*----------------------------------------------------------------
 *
 * nonvol.h
 *
 * Header file for persistent storage functions
 * VSKTarget - ESP32-S3 Port (Rev 6.0)
 *
 * Uses ESP32 Preferences library (NVS) instead of EEPROM
 *
 *---------------------------------------------------------------*/
#ifndef _NONVOL_H_
#define _NONVOL_H_

#include "wifi.h"

#define PS_VERSION        16                      // Persistent storage version
#define PS_UNINIT(x)     ( ((x) == 0xABAB) || ((x) == 0xFFFF))

/*
 * Function prototypes
 */
void factory_nonvol(bool new_serial_number);
void init_nonvol(int v);
void read_nonvol(void);
void update_nonvol(unsigned int current_version);
void gen_position(int v);
void dump_nonvol(void);
void backup_nonvol(void);
void restore_nonvol(void);
void check_nonvol(void);

/*
 * NON Vol Storage - ESP32 uses Preferences (NVS) with string keys
 * The NONVOL_* defines are kept for JSON table compatibility
 * but actual storage uses named key-value pairs via Preferences
 */
#define NONVOL_INIT           0x0
#define NONVOL_SENSOR_DIA     (NONVOL_INIT        + sizeof(int) + 2)
#define NONVOL_DIP_SWITCH     (NONVOL_SENSOR_DIA  + sizeof(double) + 2)
#define NONVOL_RH             (NONVOL_DIP_SWITCH  + sizeof(int))
#define NONVOL_PAPER_TIME     (NONVOL_RH          + sizeof(int))
#define NONVOL_TEST_MODE      (NONVOL_PAPER_TIME  + sizeof(int) + 2)
#define NONVOL_CALIBRE_X10    (NONVOL_TEST_MODE   + sizeof(int) + 2)
#define NONVOL_SENSOR_ANGLE   (NONVOL_CALIBRE_X10 + sizeof(int) + 2)
#define NONVOL_NORTH_X        (NONVOL_SENSOR_ANGLE + sizeof(int) + 2)
#define NONVOL_NORTH_Y        (NONVOL_NORTH_X     + sizeof(int) + 2)
#define NONVOL_EAST_X         (NONVOL_NORTH_Y     + sizeof(int) + 2)
#define NONVOL_EAST_Y         (NONVOL_EAST_X      + sizeof(int) + 2)
#define NONVOL_SOUTH_X        (NONVOL_EAST_Y      + sizeof(int) + 2)
#define NONVOL_SOUTH_Y        (NONVOL_SOUTH_X     + sizeof(int) + 2)
#define NONVOL_WEST_X         (NONVOL_SOUTH_Y     + sizeof(int) + 2)
#define NONVOL_WEST_Y         (NONVOL_WEST_X      + sizeof(int) + 2)
#define NONVOL_POWER_SAVE     (NONVOL_WEST_Y      + sizeof(int) + 2)
#define NONVOL_NAME_ID        (NONVOL_POWER_SAVE  + sizeof(int) + 2)
#define NONVOL_1_RINGx10      (NONVOL_NAME_ID     + sizeof(int) + 2)
#define NONVOL_LED_PWM        (NONVOL_1_RINGx10   + sizeof(int) + 2)
#define NONVOL_SEND_MISS      (NONVOL_LED_PWM     + sizeof(int) + 2)
#define NONVOL_SERIAL_NO      (NONVOL_SEND_MISS   + sizeof(int) + 2)
#define NONVOL_STEP_COUNT     (NONVOL_SERIAL_NO   + sizeof(int) + 2)
#define NONVOL_MFS            (NONVOL_STEP_COUNT  + sizeof(int) + 2)
#define NONVOL_STEP_TIME      (NONVOL_MFS         + sizeof(int) + 2)
#define NONVOL_Z_OFFSET       (NONVOL_STEP_TIME   + sizeof(int) + 2)
#define NONVOL_PAPER_ECO      (NONVOL_Z_OFFSET    + sizeof(int) + 2)
#define NONVOL_TARGET_TYPE    (NONVOL_PAPER_ECO   + sizeof(int) + 2)
#define NONVOL_TABATA_ENBL    (NONVOL_TARGET_TYPE + sizeof(int) + 2)
#define NONVOL_TABATA_ON      (NONVOL_TABATA_ENBL + sizeof(int) + 2)
#define NONVOL_TABATA_REST    (NONVOL_TABATA_ON   + sizeof(int) + 2)
#define NONVOL_TABATA_CYCLES  (NONVOL_TABATA_REST + sizeof(int) + 2)
#define NONVOL_RAPID_ON       (NONVOL_TABATA_CYCLES+sizeof(int))
#define NONVOL_RAPID_REST     (NONVOL_RAPID_ON    + sizeof(int))
#define NONVOL_RAPID_CYCLES   (NONVOL_RAPID_REST  + sizeof(int))
#define NONVOL_vset_PWM       (NONVOL_RAPID_CYCLES + sizeof(int))
#define NONVOL_VSET           (NONVOL_vset_PWM    + sizeof(int))
#define NONVOL_RAPID_TYPE     (NONVOL_VSET        + sizeof(double))
#define NONVOL_PS_VERSION     (NONVOL_RAPID_TYPE  + sizeof(int))
#define NONVOL_FOLLOW_THROUGH (NONVOL_PS_VERSION  + sizeof(int))
#define NONVOL_KEEP_ALIVE     (NONVOL_FOLLOW_THROUGH + sizeof(int))
#define NONVOL_TABATA_WARN_ON (NONVOL_KEEP_ALIVE + sizeof(int))
#define NONVOL_TABATA_WARN_OFF (NONVOL_TABATA_WARN_ON + sizeof(int))
#define NONVOL_FACE_STRIKE    (NONVOL_TABATA_WARN_OFF + sizeof(int))
#define NONVOL_RAPID_COUNT    (NONVOL_FACE_STRIKE + sizeof(int))
#define NONVOL_WIFI_CHANNEL   (NONVOL_RAPID_COUNT + sizeof(int))
#define NONVOL_WIFI_DHCP      (NONVOL_WIFI_CHANNEL + sizeof(int))
#define NONVOL_WIFI_SSID      (NONVOL_WIFI_DHCP  + sizeof(int))
#define NONVOL_WIFI_PWD       (NONVOL_WIFI_SSID  + WIFI_SSID_SIZE)
#define NONVOL_WIFI_IP        (NONVOL_WIFI_PWD   + WIFI_PWD_SIZE)
#define NONVOL_WIFI_SSID_32   (NONVOL_WIFI_IP    + WIFI_IP_SIZE)
#define NONVOL_MIN_RING_TIME  (NONVOL_WIFI_SSID_32 + WIFI_SSID_SIZE)
#define NONVOL_DOPPLER        (NONVOL_MIN_RING_TIME  + sizeof(int))
#define NONVOL_TOKEN          (NONVOL_DOPPLER    + sizeof(double))
#define NONVOL_MFS2           (NONVOL_TOKEN      + sizeof(int))

#define NONVOL_NEXT           (NONVOL_MFS2       + sizeof(double))
#define NONVOL_SIZE           4096

#endif /* _NONVOL_H_ */
