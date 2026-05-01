/*----------------------------------------------------------------
 *
 * json.h
 *
 * Header file for JSON functions and called routines
 * VSKTarget - ESP32-S3 Port (Rev 6.0)
 *
 *---------------------------------------------------------------*/
#ifndef _JSON_H_
#define _JSON_H_
#include "vskETarget.h"

typedef struct  {
  char*             token;
  int*              value;
  double*         d_value;
  byte            convert;
  void         (*f)(int x);
  unsigned int    non_vol;
  unsigned int init_value;
} json_message;

#define IS_VOID       0
#define IS_TEXT       1
#define IS_SECRET     2
#define IS_INT16      3
#define IS_FLOAT      4
#define IS_DOUBLE     5
#define IS_FIXED      6

void reset_JSON(void);
bool read_JSON(void);
void show_echo(void);

extern int    json_dip_switch;
extern double json_sensor_dia;
extern int    json_sensor_angle;
extern int    json_paper_time;
extern int    json_echo;
extern int    json_calibre_x10;
extern int    json_north_x;
extern int    json_north_y;
extern int    json_east_x;
extern int    json_east_y;
extern int    json_south_x;
extern int    json_south_y;
extern int    json_west_x;
extern int    json_west_y;
extern int    json_spare_1;
extern int    json_name_id;
extern int    json_1_ring_x10;
extern int    json_LED_PWM;
extern int    json_power_save;
extern int    json_send_miss;
extern int    json_serial_number;
extern int    json_step_count;
extern int    json_step_time;
extern int    json_multifunction;
extern int    json_z_offset;
extern int    json_paper_eco;
extern int    json_target_type;
#define FIVE_BULL_AIR_RIFLE_74 1
#define FIVE_BULL_AIR_RIFLE_79 2
#define TWELVE_BULL_AIR_RIFLE  3
extern int    json_tabata_enable;
extern int    json_tabata_on;
extern int    json_tabata_rest;
extern int    json_tabata_warn_on;
extern int    json_tabata_warn_off;
extern int    json_rapid_enable;
extern unsigned long   json_rapid_on;
extern int    json_rapid_count;
extern int    json_vset_PWM;
extern double json_vset;
extern int    json_follow_through;
extern int    json_keep_alive;
extern int    json_face_strike;
extern int    json_rapid_time;
extern int    json_wifi_channel;
extern int    json_rapid_wait;
extern int    json_wifi_dhcp;
extern char   json_wifi_ssid[];
extern char   json_wifi_pwd[];
extern char   json_wifi_ip[];
extern int    json_rh;
extern int    json_min_ring_time;
extern double json_doppler;
extern int    json_token;
extern int    json_multifunction2;
extern int    json_start_ip;
extern int    json_A, json_B, json_C, json_D;

#endif /* _JSON_H_ */
