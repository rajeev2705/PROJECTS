/*----------------------------------------------------------------
 * 
 * vskETarget.ino       
 * 
 * Software to run the Air-Rifle / Small Bore Electronic Target
 * VSKTarget - ESP32-S3 Port (Rev 6.1)
 * 
 * Based on freETarget v5.2 architecture:
 * - ESP32-S3 PCNT hardware timers (no external logic chain)
 * - A0505XT-1WR3 isolated +/-5V for sensor daughter boards
 * - Software-controlled VREF (PWM + RC filter)
 * - Sensor filtering on separate daughter boards (beside FRC)
 * - Direct GPIO for LEDs/motor (no MCP23017)
 * - Dedicated I2C bus (no pin sharing)
 * 
 *-------------------------------------------------------------*/
#include "vskETarget.h"
#include "gpio.h"
#include "compute_hit.h"
#include "analog_io.h"
#include "json.h"
#include "nonvol.h"
#include "mechanical.h"
#include "diag_tools.h"
#include "wifi.h"
#include "timer.h"
#include "token.h"

shot_record_t record[SHOT_STRING];
volatile unsigned int this_shot;
volatile unsigned int last_shot;

double        s_of_sound;
unsigned int  shot = 0;
unsigned int  face_strike = 0;
unsigned int  is_trace = 0;
unsigned int  rapid_count = 0;
unsigned int  tabata_state;
unsigned int  shot_number;
volatile unsigned long  keep_alive;
volatile unsigned long  tabata_rapid_timer;
volatile unsigned long  in_shot_timer;
volatile unsigned long  power_save;
volatile unsigned long  token_tick;
volatile unsigned long  gpt;

const char* namesensor[] = { "TARGET",
                        "1",      "2",        "3",     "4",      "5",       "6",       "7",     "8",     "9",      "10",
                        "DOC",    "DOPEY",  "HAPPY",   "GRUMPY", "BASHFUL", "SNEEZEY", "SLEEPY",
                        "RUDOLF", "DONNER", "BLITZEN", "DASHER", "PRANCER", "VIXEN",   "COMET", "CUPID", "DUNDER",
                        "ODIN",   "WODEN",   "THOR",   "BALDAR",
                        0};
                  
const char nesw[]   = "NESW";
const char to_hex[] = "0123456789ABCDEF";
static void bye(void);
static long tabata(bool reset_time);
static bool discard_tabata(void);

#define TABATA_OFF          0
#define TABATA_REST         1
#define TABATA_WARNING      2
#define TABATA_DARK         3
#define TABATA_ON           4

/*----------------------------------------------------------------
 * setup() - Initialize the board
 *--------------------------------------------------------------*/
void setup(void) 
{    
  Serial.begin(115200);
  
  /*
   * ESP32-S3 configurable UART pins
   * AUX on GPIO8(TX)/GPIO0(RX) - no conflicts!
   * Display on GPIO43(TX)/GPIO44(RX) via FT231XS
   */
  AUX_SERIAL.begin(115200, SERIAL_8N1, AUX_RX_PIN, AUX_TX_PIN);
  DISPLAY_SERIAL.begin(115200, SERIAL_8N1, DISP_RX_PIN, DISP_TX_PIN);
  
  POST_version();
  
  init_gpio();  
  init_sensors();
  init_analog_io();
  init_timer();
  set_LED('*', '.', '.');
  read_nonvol();
  
  is_trace = DLT_CRITICAL;

  timer_new(&keep_alive,    (unsigned long)json_keep_alive * ONE_SECOND);
  timer_new(&tabata_rapid_timer,   0);
  timer_new(&in_shot_timer, FULL_SCALE);
  timer_new(&power_save,    (unsigned long)(json_power_save) * (long)ONE_SECOND * 60L);
  timer_new(&token_tick,    5 * ONE_SECOND);
  timer_new(&gpt,           0);
  
  randomSeed(read_reference());
  
  set_LED('.', '.', '*');
  tabata(true);
  
  set_LED('*', '.', '*');
  while ( (POST_counters() == false)
              && !DLT(DLT_CRITICAL))
  {
    Serial.print(T("POST_2 Failed\r\n"));
    blink_fault(POST_COUNT_FAILED);
  }
  
  set_LED('.', '*', '.');
  enable_timer_interrupt();
     
  set_LED('*', '*', '.');
  wifi_init();
   
  multifunction_switch();
  set_LED('*', '*', '*');
  set_LED_PWM(json_LED_PWM);
  POST_LEDs();
  set_LED(LED_READY);
  
  while ( available_all() )
  {
    get_all();
  }
  
  DLT(DLT_CRITICAL); 
  Serial.print(T("VSKTarget ESP32-S3 Rev 6.1 startup complete\n\r"));
  Serial.print(T("Architecture: PCNT timing (no external logic)\n\r"));
  Serial.print(T("Sensors: GPIO4(N) GPIO5(E) GPIO6(S) GPIO7(W)\n\r"));
  Serial.print(T("Power: A0505XT-1WR3 isolated +-5V\n\r"));
  Serial.print(T("VREF: Software controlled (PWM+RC)\n\r"));
  Serial.print(T("Free heap: ")); Serial.print(ESP.getFreeHeap());
  Serial.print(T(" bytes\n\r"));
  show_echo();
  return;
}

/*----------------------------------------------------------------
 * loop() - Main control loop (identical logic to Rev 5.0)
 *--------------------------------------------------------------*/
#define SET_MODE      0
#define ARM        (SET_MODE+1)
#define WAIT       (ARM+1)
#define REDUCE     (WAIT+1)
#define FINISH     (REDUCE+1)

unsigned int state = SET_MODE;
unsigned int old_state = ~SET_MODE;

unsigned int  sensor_status;
unsigned int  location;

char* loop_name[] = {"SET_MODE", "ARM", "WAIT", "ACQUIRE", "REDUCE", "FINISH" };

void loop() 
{
  multifunction_switch();
  tabata(false);

  switch (json_token)
  {
    case TOKEN_WIFI:
      wifi_receive();
      break;

    case TOKEN_MASTER:
      if ( token_tick == 0 )
      {
        token_init();
        token_tick = (my_ring == TOKEN_UNDEF) ? ONE_SECOND * 5 : ONE_SECOND * 60;
      }
    case TOKEN_SLAVE:
      token_poll();
      break;
  }

  while (AUX_SERIAL.available())
  {
    aux_spool_put(AUX_SERIAL.read());
  }
    
  if ( read_JSON() )
  {
    power_save = (long)json_power_save * 60L * (long)ONE_SECOND;
  }

  if ( (json_keep_alive != 0)
    && (keep_alive == 0) )
  {
    send_keep_alive();
    keep_alive = json_keep_alive * ONE_SECOND;
  }

  if ( (json_power_save != 0) 
       && (power_save == 0) )
  {
    bye();
    power_save = (unsigned long)json_power_save * (unsigned long)ONE_SECOND * 60L;
    state = SET_MODE;
  }

  if ( (state != old_state) 
      && DLT(DLT_APPLICATION) )
  {
    Serial.print(T("Loop State: ")); Serial.print(loop_name[state]);
  } 
  old_state = state;
  
  switch (state)
  {
    default:
    case SET_MODE:
      state = set_mode();
      break;
    case ARM:
      state = arm();
      break;
    case WAIT:
      state = wait();
      break;
    case REDUCE:
      state = reduce();
      break;
    case FINISH:
      state = finish();
      break;
  }
  
  return;
}

/*----------------------------------------------------------------
 * State machine functions
 *--------------------------------------------------------------*/
unsigned int set_mode(void)
{
  unsigned int i;
  for (i = 0; i < SHOT_STRING; i++)
  {
    record[i].face_strike = 100;
  }

  if ( json_tabata_enable || json_rapid_enable )
  {
    set_LED_PWM_now(0);
    set_LED(LED_TABATA_ON);
    json_rapid_enable = 0;
  }
  else
  {
    set_LED_PWM(json_LED_PWM);
  }

  return ARM;
}
    
unsigned int arm(void)
{
  face_strike = 0;
  if ( json_send_miss && (json_face_strike != 0))
  {
    enable_face_interrupt();
  }

  stop_timers();
  arm_timers();
  
  sensor_status = is_running();
  if ( sensor_status == 0 )
  { 
    return WAIT;
  }

  if ( sensor_status & TRIP_NORTH )
  {
    Serial.print(T("\r\n{ \"Fault\": \"N\" }"));
    set_LED(NORTH_FAILED);
    delay(ONE_SECOND);
  }
  if ( sensor_status & TRIP_EAST )
  {
    Serial.print(T("\r\n{ \"Fault\": \"E\" }"));
    set_LED(EAST_FAILED);
    delay(ONE_SECOND);
  }
  if ( sensor_status & TRIP_SOUTH )
  {
    Serial.print(T("\r\n{ \"Fault\": \"S\" }"));
    set_LED(SOUTH_FAILED);
    delay(ONE_SECOND);
  }
  if ( sensor_status & TRIP_WEST )
  {
    Serial.print(T("\r\n{ \"Fault\": \"W\" }"));
    set_LED(WEST_FAILED);
    delay(ONE_SECOND);
  }

  return ARM;
}
   
static bool rapid_once;

unsigned int wait(void)
{
  if ( wifi_connected()
       || ((json_token != TOKEN_WIFI) && (my_ring != TOKEN_UNDEF)) )
  {
    set_LED(LED_READY);
  }
  else
  {
    if ( (millis() / 1000) & 1 )
      set_LED(LED_READY);
    else
      set_LED(LED_OFF);
  }

  if (json_rapid_enable == 1)
  {
    if (tabata_rapid_timer == 0)
    {
      set_LED_PWM_now(0);
      Serial.print(T("{\"RAPID_OVER\": 0}\r\n"));
      return FINISH;
    }
    else
    {
      if ( rapid_once )
      {
        Serial.print(T("{\"RAPID_START\": ")); Serial.print(json_rapid_time); Serial.print(T("}\r\n"));
        rapid_once = false;
      }
      set_LED_PWM_now(json_LED_PWM);
    }
  }

  if ( this_shot != last_shot )
  {
    return REDUCE;
  }
  
  return WAIT;
}

unsigned int reduce(void)
{
  if ( discard_shot() )
  {
    last_shot = this_shot;
    send_miss(&record[last_shot]);
    return FINISH;
  }
  
  while (last_shot != this_shot)
  {   
    location = compute_hit(&record[last_shot]);

    if ( location != MISS )
    {
      if ((json_rapid_enable == 0) && (json_tabata_enable == 0))
      {
        delay(ONE_SECOND * json_follow_through);
      }
      send_score(&record[last_shot]);
      rapid_red(0);
      rapid_green(1);

      if ( (json_paper_time + json_step_time) != 0 )
      {
        if ( ((json_paper_eco == 0)
            || ( sqrt(sq(record[last_shot].xphys_mm) + sq(record[last_shot].yphys_mm)) < json_paper_eco )) )
        {
          drive_paper();
        }
      } 
    }
    else
    {
      blink_fault(SHOT_MISS);
      send_miss(&record[last_shot]);
      rapid_green(0);
      rapid_red(1);
    }

    if ( rapid_count != 0 ) rapid_count--;
    
    if ( tabata_rapid_timer != 0 )
    {
      if ( rapid_count == 0 )
      {
        last_shot = this_shot;
        break;
      }
    }
    
    last_shot = (last_shot + 1) % SHOT_STRING;
  }

  if ( tabata_rapid_timer == 0 )
    return FINISH;
  else
    return WAIT;
}
    
unsigned int finish(void)
{
  power_save = (long)json_power_save * 60L * (long)ONE_SECOND;     
  return SET_MODE;
}

/*----------------------------------------------------------------
 * tabata, rapid fire, power management
 *--------------------------------------------------------------*/
void tabata_enable(unsigned int enable)
{
  char str[32];
  if ( enable != 0 )
    set_LED_PWM_now(0);
  else
    set_LED_PWM_now(json_LED_PWM);
  
  tabata_state = TABATA_OFF;
  json_tabata_enable = enable;
  sprintf(str, "{\"TABATA_ENABLED\":%d}\r\n", enable);
  output_to_all(str);
  return;
}

static uint16_t old_tabata_state = ~TABATA_OFF;

static long tabata(bool reset_time)
{
  char s[32];

  if ( (json_tabata_enable == 0) || reset_time )
  {
    tabata_state = TABATA_OFF;
  }

  switch (tabata_state)
  {
    case (TABATA_OFF):
      if ( json_tabata_enable != 0 )
      {
        tabata_rapid_timer = 30 * ONE_SECOND;
        set_LED_PWM_now(0);
        sprintf(s, "{\"TABATA_STARTING\":%d}\r\n", 30);
        output_to_all(s);
        tabata_state = TABATA_REST;
      }
      break;
      
    case (TABATA_REST):
      if (tabata_rapid_timer == 0)
      {
        tabata_rapid_timer = json_tabata_warn_on * ONE_SECOND;
        set_LED_PWM_now(json_LED_PWM);
        tabata_state = TABATA_WARNING;
      }
      break;
      
    case (TABATA_WARNING):
      if ( (tabata_rapid_timer % 50) == 0 )
      {
        set_LED_PWM_now(((tabata_rapid_timer / 50) & 1) ? json_LED_PWM : 0);
      }
      if (tabata_rapid_timer == 0)
      {
        tabata_rapid_timer = json_tabata_warn_off * ONE_SECOND;
        set_LED_PWM_now(0);
        tabata_state = TABATA_DARK;
      }
      break;
    
    case (TABATA_DARK):
      if (tabata_rapid_timer == 0)
      {
        in_shot_timer = FULL_SCALE;
        tabata_rapid_timer = json_tabata_on * ONE_SECOND;
        set_LED_PWM_now(json_LED_PWM);
        tabata_state = TABATA_ON;
      }
      break;
    
    case (TABATA_ON):
      if ( tabata_rapid_timer == 0 )
      {
        tabata_rapid_timer = ((long)(json_tabata_rest - json_tabata_warn_on - json_tabata_warn_off) * ONE_SECOND);
        set_LED_PWM_now(0);
        tabata_state = TABATA_REST;
      }
      break;
  }

  old_tabata_state = tabata_state;
  return 0;
}

bool discard_shot(void)
{
  if ( (json_rapid_enable != 0) && (rapid_count == 0) )
  {
    last_shot = this_shot;
    return true;
  }

  if ( (json_tabata_enable != 0) && ( tabata_state != TABATA_ON ) )
  {
    last_shot = this_shot;
    return true;
  }
  
  return false;
}

#define RANDOM_INTERVAL 100

void rapid_enable(unsigned int enable)
{
  char str[32];
  long random_wait;

  if ( enable != 0 )
  {
    set_LED_PWM_now(0);
    rapid_red(1);
    rapid_green(0);
    
    if ( json_rapid_wait != 0 )
    {      
      if ( json_rapid_wait >= RANDOM_INTERVAL )
        random_wait = random(5, json_rapid_wait % 100);
      else
        random_wait = json_rapid_wait;
      
      sprintf(str, "{\"RAPID_WAIT\":%d}\r\n", (int)random_wait);
      output_to_all(str);
      delay(random_wait * ONE_SECOND);
    }
    
    tabata_rapid_timer = (long)json_rapid_time * ONE_SECOND;
    in_shot_timer = FULL_SCALE;
    rapid_count = json_rapid_count;
    shot_number = 1;
  }
  else
  {
    rapid_red(0);
  }
   
  set_LED_PWM_now(json_LED_PWM);
  json_rapid_enable = enable;

  sprintf(str, "{\"RAPID_ENABLED\":%d}\r\n", enable);
  output_to_all(str);
  return;
}

void bye(void)
{
  char str[32];

  if ( json_token != TOKEN_WIFI ) return;
  
  sprintf(str, "{\"GOOD_BYE\":0}");
  output_to_all(str);
  delay(ONE_SECOND);
  tabata_enable(false);
  rapid_enable(false);
  set_LED_PWM(LED_PWM_OFF);
  
  while ( available_all() ) get_all();
  
  while( available_all() == 0 && is_running() == 0 )
  {
    wifi_receive();
  }
  
  hello();
  return;
}

void hello(void)
{
  char str[128];
  sprintf(str, "{\"Hello_World\":0}");
  output_to_all(str);
  set_LED_PWM_now(json_LED_PWM);
  power_save = json_power_save * ONE_SECOND * 60L;
  return;
}

void send_keep_alive(void)
{
  char str[32];
  static int keep_alive_count = 0;

  if ( wifi_connected() )
  {
    sprintf(str, "{\"KEEP_ALIVE\":%d}", keep_alive_count++);
    output_to_all(str);
  }
  return;
}
