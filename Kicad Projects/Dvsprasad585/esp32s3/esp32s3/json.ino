/*-------------------------------------------------------
 * 
 * JSON.ino
 * 
 * JSON driver
 * VSKTarget - ESP32-S3 Port (Rev 6.0)
 * 
 * Changes: EEPROM replaced with NVS helper functions
 * 
 * ----------------------------------------------------*/
#include "vskETarget.h"
#include "nonvol.h"

/*
 * Forward declarations for NVS helpers defined in nonvol.ino
 */
extern void nvs_put_int(unsigned int addr, int value);
extern int  nvs_get_int(unsigned int addr, int default_val);
extern void nvs_put_double(unsigned int addr, double value);
extern void nvs_put_char(unsigned int addr, char value);

static char input_JSON[256];

int     json_dip_switch;
double  json_sensor_dia = DIAMETER;
int     json_sensor_angle;
int     json_paper_time = 0;
int     json_echo;
double  json_d_echo;
int     json_calibre_x10;
int     json_north_x;
int     json_north_y;
int     json_east_x;
int     json_east_y;
int     json_south_x;
int     json_south_y;
int     json_west_x;
int     json_west_y;
int     json_name_id;
int     json_1_ring_x10;
int     json_LED_PWM;
int     json_power_save;
int     json_send_miss;
int     json_serial_number;
int     json_step_count;
int     json_step_time;
int     json_multifunction;
int     json_z_offset;
int     json_paper_eco;
int     json_target_type;
int     json_tabata_enable;
int     json_tabata_on;
int     json_tabata_rest;
unsigned long json_rapid_on;
int     json_vset_PWM;
double  json_vset;
int     json_follow_through;
int     json_keep_alive;
int     json_tabata_warn_on;
int     json_tabata_warn_off;
int     json_face_strike;
int     json_wifi_channel;
int     json_rapid_count;
int     json_rapid_enable;
int     json_rapid_time;
int     json_rapid_wait;
char    json_wifi_ssid[WIFI_SSID_SIZE];
char    json_wifi_pwd[WIFI_PWD_SIZE];
int     json_wifi_dhcp;
int     json_rh;
int     json_min_ring_time;
double  json_doppler;
int     json_token;
int     json_multifunction2;

int     json_A, json_B, json_C, json_D;
int     json_spare_1;
int     json_start_ip;
char    json_wifi_ip[WIFI_IP_SIZE];

#define JSON_DEBUG false

       void show_echo(void);
static void show_test(int v);
static void show_test0(int v);
static void show_names(int v);
static void nop(void);
static void set_trace(int v);

  
const json_message JSON[] = {
//    token                 value stored in RAM     double stored in RAM        convert    service fcn()     NONVOL location      Initial Value
  {"\"ANGLE\":",          &json_sensor_angle,                0,                IS_INT16,  0,                NONVOL_SENSOR_ANGLE,    45 },
  {"\"BYE\":",            0,                                 0,                IS_VOID,   &bye,                             0,       0 },
  {"\"CAL\":",            0,                                 0,                IS_VOID,   &set_trip_point,                  0,       0 },
  {"\"CALIBREx10\":",     &json_calibre_x10,                 0,                IS_INT16,  0,                NONVOL_CALIBRE_X10,     45 },
  {"\"DELAY\":",          0,                                 0,                IS_INT16,  &diag_delay,                      0,       0 },
  {"\"DIP\":",            &json_dip_switch,                  0,                IS_INT16,  0,                NONVOL_DIP_SWITCH,       0 },
  {"\"DOPPLER\":",        0,                     &json_doppler,                IS_FLOAT,  0,                NONVOL_DOPPLER,         50 },
  {"\"ECHO\":",           0,                                 0,                IS_VOID,   &show_echo,                       0,       0 },
  {"\"FACE_STRIKE\":",    &json_face_strike,                 0,                IS_INT16,  0,                NONVOL_FACE_STRIKE,      0 },
  {"\"FOLLOW_THROUGH\":", &json_follow_through,              0,                IS_INT16,  0,                NONVOL_FOLLOW_THROUGH,   0 },
  {"\"INIT\":",           0,                                 0,                IS_INT16,  &init_nonvol,     NONVOL_INIT,             0 },
  {"\"KEEP_ALIVE\":",     &json_keep_alive,                  0,                IS_INT16,  0,                NONVOL_KEEP_ALIVE,     120 },
  {"\"LED_BRIGHT\":",     &json_LED_PWM,                     0,                IS_INT16,  &set_LED_PWM_now, NONVOL_LED_PWM,         50 },
  {"\"MFS\":",            &json_multifunction,               0,                IS_INT16,  0,                NONVOL_MFS,  (LED_ADJUST*10000) 
                                                                                                                          + (POWER_TAP * 1000)
                                                                                                                          + (PAPER_SHOT * 100) 
                                                                                                                          + (ON_OFF * 10) 
                                                                                                                          + (PAPER_FEED) },
  {"\"MFS2\":",            &json_multifunction2,             0,                IS_INT16,  0,                NONVOL_MFS2,  0 },
  {"\"MIN_RING_TIME\":",  &json_min_ring_time,               0,                IS_INT16,  0,                NONVOL_MIN_RING_TIME,  500 },
  {"\"NAME_ID\":",        &json_name_id,                     0,                IS_INT16,  &show_names,      NONVOL_NAME_ID,          0 },
  {"\"NEW\":",            &shot_number,                      0,                IS_INT16,  0,                0,                       0 },
  {"\"NONVOL_BACKUP\":",  0,                                 0,                IS_VOID,   &backup_nonvol,   0,                       0 },
  {"\"NONVOL_RESTORE\":", 0,                                 0,                IS_VOID,   &restore_nonvol,  0,                       0 },
  {"\"PAPER_ECO\":",      &json_paper_eco,                   0,                IS_INT16,  0,                NONVOL_PAPER_ECO,        0 },
  {"\"PAPER_TIME\":",     &json_paper_time,                  0,                IS_INT16,  0,                NONVOL_PAPER_TIME,      50 },
  {"\"POWER_SAVE\":",     &json_power_save,                  0,                IS_INT16,  0,                NONVOL_POWER_SAVE,      30 },
  {"\"RAPID_COUNT\":",    &json_rapid_count,                 0,                IS_INT16,  0,                0,                       0 },
  {"\"RAPID_ENABLE\":",   &json_rapid_enable,                0,                IS_INT16,  &rapid_enable,    0,                       0 },
  {"\"RAPID_TIME\":",     &json_rapid_time,                  0,                IS_INT16,  0,                0,                       0 },
  {"\"RAPID_WAIT\":",     &json_rapid_wait,                  0,                IS_INT16,  0,                0,                       0 },
  {"\"RESET\":",          0,                                 0,                IS_INT16,  &setup,           0,                       0 },
  {"\"RH\":",             &json_rh,                          0,                IS_INT16,  0,                NONVOL_RH,              50 },
  {"\"SEND_MISS\":",      &json_send_miss,                   0,                IS_INT16,  0,                NONVOL_SEND_MISS,        0 },
  {"\"SENSOR\":",         0,                                 &json_sensor_dia, IS_FLOAT,  &gen_position,    NONVOL_SENSOR_DIA,     230 },
  {"\"SN\":",             &json_serial_number,               0,                IS_FIXED,  0,                NONVOL_SERIAL_NO,   0xffff },
  {"\"STEP_COUNT\":",     &json_step_count,                  0,                IS_INT16,  0,                NONVOL_STEP_COUNT,       0 },
  {"\"STEP_TIME\":",      &json_step_time,                   0,                IS_INT16,  0,                NONVOL_STEP_TIME,        0 },
  {"\"TABATA_ENABLE\":",  &json_tabata_enable,               0,                IS_INT16,  &tabata_enable,   0,                       0 },
  {"\"TABATA_ON\":",      &json_tabata_on,                   0,                IS_INT16,  0,                0,                       0 },
  {"\"TABATA_REST\":",    &json_tabata_rest,                 0,                IS_INT16,  0,                0,                       0 },
  {"\"TABATA_WARN_OFF\":",&json_tabata_warn_off,             0,                IS_INT16,  0,                0,                       0 },
  {"\"TABATA_WARN_ON\":", &json_tabata_warn_on,              0,                IS_INT16,  0,                0,                     200 },
  {"\"TARGET_TYPE\":",    &json_target_type,                  0,                IS_INT16,  0,                NONVOL_TARGET_TYPE,      0 },
  {"\"TEST\":",           0,                                 0,                IS_INT16,  &show_test,       NONVOL_TEST_MODE,        0 },
  {"\"TOKEN\":",          &json_token,                       0,                IS_INT16,  0,                NONVOL_TOKEN,            0 },
  {"\"TRACE\":",          0,                                 0,                IS_INT16,  &set_trace,       0,                       0 },
  {"\"VERSION\":",        0,                                 0,                IS_INT16,  &POST_version,    0,                       0 },
  {"\"V_SET\":",          0,                                 &json_vset,       IS_FLOAT,  &compute_vset_PWM,NONVOL_VSET,             0 },
  {"\"WIFI_CHANNEL\":",   &json_wifi_channel,                0,                IS_INT16,  0,                NONVOL_WIFI_CHANNEL,     6 },
  {"\"WIFI_PWD\":",       (int*)&json_wifi_pwd,              0,                IS_SECRET, 0,                NONVOL_WIFI_PWD,         0 },
  {"\"WIFI_SSID\":",      (int*)&json_wifi_ssid,             0,                IS_TEXT,   0,                NONVOL_WIFI_SSID_32,     0 },
  {"\"Z_OFFSET\":",       &json_z_offset,                    0,                IS_INT16,  0,                NONVOL_Z_OFFSET,        13 },
  {"\"NORTH_X\":",        &json_north_x,                     0,                IS_INT16,  0,                NONVOL_NORTH_X,          0 },
  {"\"NORTH_Y\":",        &json_north_y,                     0,                IS_INT16,  0,                NONVOL_NORTH_Y,          0 },
  {"\"EAST_X\":",         &json_east_x,                      0,                IS_INT16,  0,                NONVOL_EAST_X,           0 },
  {"\"EAST_Y\":",         &json_east_y,                      0,                IS_INT16,  0,                NONVOL_EAST_Y,           0 },
  {"\"SOUTH_X\":",        &json_south_x,                     0,                IS_INT16,  0,                NONVOL_SOUTH_X,          0 },
  {"\"SOUTH_Y\":",        &json_south_y,                     0,                IS_INT16,  0,                NONVOL_SOUTH_Y,          0 },
  {"\"WEST_X\":",         &json_west_x,                      0,                IS_INT16,  0,                NONVOL_WEST_X,           0 },
  {"\"WEST_Y\":",         &json_west_y,                      0,                IS_INT16,  0,                NONVOL_WEST_Y,           0 },
  {"\"TA\":",             &json_A,                           0,                IS_INT16,  0,                0,                       0 },
  {"\"TB\":",             &json_B,                           0,                IS_INT16,  0,                0,                       0 },
  {"\"TC\":",             &json_C,                           0,                IS_INT16,  0,                0,                       0 },
  {"\"TD\":",             &json_D,                           0,                IS_INT16,  0,                0,                       0 },
  { 0, 0, 0, 0, 0, 0}
};

int instr(char* s1, char* s2);
static void diag_delay(int x) { Serial.print(T("\r\n\"DELAY\":")); Serial.print(x); delay(x*1000); return;}

/*-----------------------------------------------------
 * read_JSON - Accumulate and parse JSON input
 * 
 * Identical logic to Arduino version, but EEPROM calls
 * replaced with nvs_put_* helpers
 *-----------------------------------------------------*/
static uint16_t in_JSON = 0;
static int16_t got_right = false;
static bool not_found;
static bool keep_space;

static int to_int(char h)
{
  h = toupper(h);
  if ( h > '9' ) return 10 + (h-'A');
  else return h - '0';
}

bool read_JSON(void)
{
  unsigned int i, j;
  int k;
  unsigned int m;
  unsigned int x;
  char* s;
  double y;
  bool return_value;
  char ch;
  
  return_value = false;

  while ( available_all() != 0 )
  {
    return_value = true;
    ch = get_all();
    if ( ch == '*' ) ch = '"';
    if ( ch == '?' ) { show_echo(); return return_value; }

    switch (ch)
    {        
      case '%': self_test(T_XFR_LOOP); break;
      case '^': drive_paper(); break;
      case '{': in_JSON = 0; input_JSON[0] = 0; got_right = 0; keep_space = 0; break;
      case '}': if ( in_JSON != 0 ) got_right = in_JSON; break;
      case 0x08: if ( in_JSON != 0 ) in_JSON--; input_JSON[in_JSON] = 0; break;
      case '"': keep_space = (keep_space ^ 1) & 1; /* FALLTHROUGH */
      default:
        if ( (ch != ' ') || keep_space )
        {
          input_JSON[in_JSON] = ch;
          if ( in_JSON < (sizeof(input_JSON)-1) ) in_JSON++;
        }
        break;
    }
    input_JSON[in_JSON] = 0;
  }
  
  if ( got_right == 0 ) return return_value;
  
  not_found = true;
  for ( i=0; i < (unsigned int)got_right; i++)
  {
    j = 0;
    while ( JSON[j].token != 0 )
    {
      k = instr(&input_JSON[i], JSON[j].token);
      if ( k > 0 )
      {
        not_found = false;
        switch ( JSON[j].convert )
        {
          default:
          case IS_VOID:
          case IS_FIXED:
            x = 0; y = 0;
            break;
                    
          case IS_TEXT:
          case IS_SECRET:
            while ( input_JSON[i+k] != '"' ) k++;
            k++;
            s = (char *)JSON[j].value;
            *s = 0;
            m = 0;
            if (JSON[j].non_vol != 0) nvs_put_char(JSON[j].non_vol, 0);
            while ( input_JSON[i+k] != '"' )
            {
              if ( s != 0 ) { *s = input_JSON[i+k]; s++; *s = 0; }
              if ( JSON[j].non_vol != 0 )
              {
                nvs_put_char(JSON[j].non_vol+m, input_JSON[i+k]);
                m++;
                nvs_put_char(JSON[j].non_vol+m, 0);
              }
              k++;
            }
            break;
            
          case IS_INT16:
            if ( (input_JSON[i+k] == '0') && ( (input_JSON[i+k+1] == 'X') || (input_JSON[i+k+1] == 'x')) )
            {
              x = (to_int(input_JSON[i+k+2]) << 4) + to_int(input_JSON[i+k+3]);
            }
            else
            {
              x = atoi(&input_JSON[i+k]);
            }
            if ( JSON[j].value != 0 ) *JSON[j].value = x;
            if ( JSON[j].non_vol != 0 ) nvs_put_int(JSON[j].non_vol, x);
            break;

          case IS_FLOAT:
          case IS_DOUBLE:
            y = atof(&input_JSON[i+k]);
            if ( JSON[j].d_value != 0 ) *JSON[j].d_value = y;
            if ( JSON[j].non_vol != 0 ) nvs_put_double(JSON[j].non_vol, y);
            break;
        }
        
        if ( JSON[j].f != 0 ) JSON[j].f(x);
      }
      j++;
    }
  }

  if ( not_found == true )
  {
    Serial.print(T("\r\n\r\nCannot decode: {")); Serial.print(input_JSON); Serial.print(T("}"));
  }
  
  in_JSON = 0;
  got_right = 0;
  input_JSON[in_JSON] = 0;
  return return_value;
}

int instr(char* s1, char* s2)
{
  int i = 0;
  while ( (*s1 != 0) && (*s2 != 0) )
  {
    if ( *s1 != *s2 ) return -1;
    s1++; s2++; i++;
  }
  if ( *s2 == 0 ) return i;
  return -1;
}

/*-----------------------------------------------------
 * show_echo - Display current settings
 *-----------------------------------------------------*/
void show_echo(void)
{
  unsigned int i, j;
  char s[512], str_c[32];

  sprintf(s, "\r\n{\r\n\"NAME\":\"%s\", \r\n", namesensor[json_name_id]);
  output_to_all(s);

  i=0;
  while ( JSON[i].token != 0 )
  {
    if ( (JSON[i].value != NULL) || (JSON[i].d_value != NULL) )
    {
      switch ( JSON[i].convert )
      {
        default:
        case IS_VOID: break;
        case IS_TEXT:
        case IS_SECRET:
          j = 0;
          while ( *((char*)(JSON[i].value)+j) != 0)
          {
            str_c[j] = (JSON[i].convert == IS_SECRET) ? '*' : *((char*)(JSON[i].value)+j);
            j++;
          }
          str_c[j] = 0;
          sprintf(s, "%s \"%s\", \r\n", JSON[i].token, str_c);
          break;
        case IS_INT16:
        case IS_FIXED:
          sprintf(s, "%s %d, \r\n", JSON[i].token, *JSON[i].value);
          break;
        case IS_FLOAT:
        case IS_DOUBLE:
          dtostrf(*JSON[i].d_value, 8, 6, str_c);
          sprintf(s, "%s %s, \r\n", JSON[i].token, str_c);
          break;
      }
      output_to_all(s);
    }
    i++;
  }
  
  // ESP32-specific status
  sprintf(s, "\n\r\"TRACE\": %d, \n\r", is_trace);
  output_to_all(s);
  sprintf(s, "\"RUNNING_MINUTES\": %ld, \n\r", millis()/1000/60);
  output_to_all(s);
  dtostrf(temperature_C(), 4, 2, str_c);
  sprintf(s, "\"TEMPERATURE\": %s, \n\r", str_c);
  output_to_all(s);
  dtostrf(speed_of_sound(temperature_C(), json_rh), 4, 2, str_c);
  sprintf(s, "\"SPEED_SOUND\": %s, \n\r", str_c);
  output_to_all(s);
  dtostrf(TO_VOLTS(read_reference()), 4, 2, str_c);   // ADS1115 #2
  sprintf(s, "\"V_REF\": %s, \n\r", str_c);
  output_to_all(s);
  
  sprintf(s, "\"WiFi_PRESENT\": 1, \n\r");
  output_to_all(s);
  wifi_myIP(str_c);
  sprintf(s, "\"WiFi_IP_ADDRESS\": \"%s:1090\", \n\r", str_c);
  output_to_all(s);
  sprintf(s, "\"FREE_HEAP\": %d, \n\r", ESP.getFreeHeap());
  output_to_all(s);
  sprintf(s, "\"VERSION\": %s, \n\r", SOFTWARE_VERSION);
  output_to_all(s);
  dtostrf(revision()/100.0, 4, 2, str_c);
  sprintf(s, "\"BD_REV\": %s \n\r", str_c);
  output_to_all(s);
  sprintf(s, "}\r\n"); 
  output_to_all(s);
  
  return;
}

static void show_names(int v)
{
  unsigned int i;
  if ( v != 0 ) return;
  Serial.print(T("\r\nNames\r\n"));
  i=0;
  while (namesensor[i] != 0)
  {
    Serial.print(i); Serial.print(T(": \"")); Serial.print(namesensor[i]); Serial.print(T("\", \r\n"));
    i++;
  }
  return;
}

static void show_test(int test_number)
{
  Serial.print(T("\r\nSelf Test:")); Serial.print(test_number); Serial.print(T("\r\n"));
  self_test(test_number);
  return;
}

static void set_trace(int trace)
{
  char s[32];
  trace |= DLT_CRITICAL;
  if ( trace & DLT_CRITICAL)    {sprintf(s, "\r\nDLT CRITICAL");    output_to_all(s);}
  if ( trace & DLT_APPLICATION) {sprintf(s, "\r\nDLT APPLICATION"); output_to_all(s);}
  if ( trace & DLT_DIAG)        {sprintf(s, "\r\nDLT DIAG");        output_to_all(s);}
  if ( trace & DLT_INFO)        {sprintf(s, "\r\nDLT INFO");        output_to_all(s);}
  is_trace = trace;
  return;   
}
