/*----------------------------------------------------------------
 *
 * diag_tools.ino
 *
 * Debug and test tools
 * VSKTarget - ESP32-S3 Port (Rev 6.1)
 *
 * Rev 6.1 Changes:
 * - TICK macro uses TICKS_PER_US (1.0) instead of OSCILLATOR_MHZ
 * - POST_counters uses PCNT self-test (trip_timers + read_timers)
 * - read_counter() calls removed (no 74LV8154)
 * - All timing in microseconds
 *
 *---------------------------------------------------------------*/

#include "vskETarget.h"
#include "gpio.h"
#include "token.h"
#include "json.h"

const char* which_one[4] = {"North:", "East:", "South:", "West:"};

/*
 * Rev 6.1: TICK converts mm position to microsecond ticks
 * s_of_sound is mm/us, so: ticks = distance_mm / s_of_sound
 */
#define TICK(x) (((x) / 0.33) * TICKS_PER_US)
#define RX(Z,X,Y) (16000 - (sqrt(sq(TICK(x)-sensor[(Z)].x_tick) + sq(TICK(y)-sensor[(Z)].y_tick))))
#define GRID_SIDE 25
#define TEST_SAMPLES ((GRID_SIDE)*(GRID_SIDE))

static void show_analog_on_PC(int v);
static void unit_test(unsigned int mode);
static bool sample_calculations(unsigned int mode, unsigned int sample);
static void log_sensor(int sensor);
extern int  json_clock[4];

unsigned int tick;

void self_test(uint16_t test)
{
  unsigned int i;
  char ch;
  unsigned int sensor_status;
  unsigned long sample;
  unsigned int random_delay;
  bool pass;
  shot_record_t shot;
  unsigned char s[128];
  
  tick++;
  
  switch (test)
  {
    default:
    case T_HELP:                
      Serial.print(T("\r\n 1 - Digital inputs"));
      Serial.print(T("\r\n 2 - Sensor trigger (external)"));
      Serial.print(T("\r\n 3 - PCNT self-test (internal trigger)"));
      Serial.print(T("\r\n 4 - Oscilloscope"));
      Serial.print(T("\r\n 6 - Advance paper backer"));
      Serial.print(T("\r\n 7 - Spiral Unit Test"));
      Serial.print(T("\r\n 8 - Grid calibration pattern"));
      Serial.print(T("\r\n 9 - One time calibration pattern"));
      Serial.print(T("\r\n11 - Calibrate")); 
      Serial.print(T("\r\n13 - Serial port test"));
      Serial.print(T("\r\n14 - LED brightness test"));
      Serial.print(T("\r\n15 - Face strike test"));
      Serial.print(T("\r\n16 - WiFi test"));
      Serial.print(T("\r\n17 - Dump NonVol"));
      Serial.print(T("\r\n18 - Send sample shot record"));
      Serial.print(T("\r\n19 - Show WiFi status"));
      Serial.print(T("\r\n26 - Speed of Sound test"));
      Serial.print(T("\r\n27 - Token Ring Test"));
      Serial.print(T("\r\n28 - LED cycle test"));
      Serial.print(T("\r\n29 - Force calculations"));
      Serial.print(T("\r\n"));
      break;

    case T_DIGITAL: 
      digital_test();
      break;

    case T_TRIGGER:
    case T_CLOCK:
      stop_timers();
      arm_timers();
      set_LED(L('*', '-', '-'));
      
      if ( test == T_CLOCK )
      {
        random_delay = random(1, 6000);
        Serial.print(T("\r\nPCNT self-test: ")); Serial.print(random_delay); Serial.print(T("us "));
        trip_timers();
        delayMicroseconds(random_delay);
      }
  
      while ( !is_running() ) { continue; }
      sensor_status = is_running();
      stop_timers();
      read_timers(&shot.timer_count[0]);

      if ( test == T_CLOCK )
      {
        /* Rev 6.1: timer values are microseconds, compare directly */
        sample = shot.timer_count[N];
        pass = true;
        for (i = N; i <= W; i++)
        {
          /* Allow 50us tolerance for ISR latency */
          if ( abs((int)shot.timer_count[i] - (int)sample) > 50 ) pass = false;
        }
        Serial.print(pass ? T(" PASS\r\n") : T(" FAIL\r\n"));
      }
      send_timer(sensor_status);
      set_LED(L('-', '-', '-'));
      delay(ONE_SECOND);
      break;

    case T_OSCOPE:
      show_analog(0);                  
      break;

    case T_PAPER: 
      Serial.print(T("\r\nAdvancing backer paper"));
      drive_paper();
      Serial.print(T("\r\nDone"));
      break;

    case T_SPIRAL: 
      Serial.print(T("\r\nSpiral Calculation\r\n"));
      unit_test(T_SPIRAL);
      break;

    case T_GRID:
      Serial.print(T("\r\nGrid Calculation\r\n"));
      unit_test(T_GRID);
      break;  
      
    case T_ONCE:
      Serial.print(T("\r\nSingle Calculation\r\n"));
      unit_test(T_ONCE);
      break;

    case T_SET_TRIP:
      set_trip_point(0);
      break;

    case T_SERIAL_PORT:
      Serial.print(T("\r\nUSB Serial: Hello World\r\n"));
      AUX_SERIAL.print(T("\r\nAUX Serial: Hello World\r\n"));
      DISPLAY_SERIAL.print(T("\r\nDisplay Serial: Hello World\r\n"));
      break;

    case T_LED:
      Serial.print(T("\r\nRamping the LED"));
      for (i = 0; i < 256; i++)
      {
        ledcWrite(LED_PWM_CHANNEL, i);
        delay(ONE_SECOND / 50);
      }
      for (i = 255; i != (unsigned int)-1; i--)
      {
        ledcWrite(LED_PWM_CHANNEL, i);
        delay(ONE_SECOND / 50);
      }
      ledcWrite(LED_PWM_CHANNEL, 0);
      Serial.print(T(" Done\r\n"));
      break;

    case T_FACE:
      Serial.print(T("\r\nFace strike test\n\r"));
      face_strike = 0;
      sample = 0;
      enable_face_interrupt();
      ch = 0;
      while ( ch != '!' )
      {        
        if ( face_strike != 0 )
        {
          face_strike--;
          set_LED(L('*', '*', '*'));
        }
        else
        {
          set_LED(L('.', '.', '.'));
        }
        if ( sample != face_strike )
        {
          Serial.print(T(" S:")); Serial.print(face_strike);
          sample = face_strike;        
        }
        wifi_receive();
        ch = get_all();
      }
      Serial.print(T("\r\nDone\n\r"));
      break;

    case T_WIFI:
      wifi_test();
      break;

    case T_NONVOL:
      dump_nonvol();
      break;

    case T_SHOT:
      if (json_B == 0) json_B = 1;
      shot.shot_number = shot_number;
      for (i = 0; i < json_B; i++)
      {
        if ( json_A != 0 )
        {
          shot.xphys_mm = (double)json_A / 2.0d - (double)random(json_A);
          shot.yphys_mm = (double)json_A / 2.0d - (double)random(json_A);
        }
        else
        {
          shot.xphys_mm = 10;
          shot.yphys_mm = 20;
        }
        shot.shot_time = (FULL_SCALE - in_shot_timer);
        send_score(&shot);
        shot.shot_number++;
      }
      break;

    case T_WIFI_STATUS:
      wifi_status();
      break;

    case T_S_OF_SOUND:
      sound_test();
      Serial.print(T("\n\rDone"));
      break;

    case T_TOKEN:
      token_init();
      Serial.print(T("\n\rDone"));
      break;

    case T_LED_CYCLE:
      i = 0;
      while ( !json_spool_available() )
      {
        set_LED((i >> 0) & 1, (i >> 1) & 1, (i >> 2) & 1);
        delay(ONE_SECOND / 2);
        i++;
        token_poll();
      }
      Serial.print(T("\n\rDone"));
      break;

    case T_FORCE_CALC:
      Serial.print(T("\r\nShot Test using entered counts (microseconds)."));
      is_trace = 255;
      set_mode();
      arm();
      this_shot = 1;
      last_shot = 0;
      record[last_shot].shot_number = 99;
      record[last_shot].xphys_mm = 0;
      record[last_shot].yphys_mm = 0;
      if ( json_A != 0 )
      {
        /* Rev 6.1: Values are microseconds */
        record[last_shot].timer_count[N] = json_A;
        record[last_shot].timer_count[E] = json_B;
        record[last_shot].timer_count[S] = json_C;
        record[last_shot].timer_count[W] = json_D;
      }
      else
      {
        /* Default test values in microseconds */
        record[last_shot].timer_count[N] = 250;
        record[last_shot].timer_count[E] = 272;
        record[last_shot].timer_count[S] = 260;
        record[last_shot].timer_count[W] = 251;
      }
      record[last_shot].face_strike = 0;
      record[last_shot].sensor_status = 0xf;
      record[last_shot].shot_time = micros();
      reduce();
      finish();
      is_trace = 0;
      Serial.print(T("\r\nDone."));
      break;
  }
    
  return;
}

/*----------------------------------------------------------------
 * POST functions
 *--------------------------------------------------------------*/
void POST_version(void)
{
  char str[64];
  sprintf(str, "\r\nVSKTarget %s\r\n", SOFTWARE_VERSION);
  output_to_all(str);
  return;
}

void POST_LEDs(void)
{
  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("POST LEDs"));
  }
  set_LED(L('*', '.', '.'));
  delay(ONE_SECOND / 4);
  set_LED(L('.', '*', '.'));
  delay(ONE_SECOND / 4);
  set_LED(L('.', '.', '*'));
  delay(ONE_SECOND / 4);
  set_LED(L('.', '.', '.'));
  return;
}

/*----------------------------------------------------------------
 * POST_counters - Rev 6.1: PCNT self-test
 * Uses trip_timers() to simulate sensor pulses and verifies
 * that all 4 PCNT channels capture timestamps correctly.
 * No read_counter() calls (74LV8154 removed).
 *--------------------------------------------------------------*/
bool POST_counters(void)
{
  unsigned int i, j;
  unsigned int random_delay;
  unsigned int sensor_status;
  unsigned int timer_counts[4];
  bool test_passed;

  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("POST_counters() - PCNT self-test"));
  }

  test_passed = true;
  
  /* Test 1: Verify no spurious triggers after arming */
  stop_timers();
  arm_timers();
  delay(1);
  sensor_status = is_running();
  if ( sensor_status != 0 )
  {
    if ( DLT(DLT_CRITICAL) )
    {
      Serial.print(T("\r\nFailed: Spurious trigger after arm"));
    }
    return false;
  }
  
  /* Test 2: Verify all sensors trigger and timestamps are reasonable */
  for (i = 0; i < 5; i++)
  {
    stop_timers();
    arm_timers();
    delay(1);
    
    /* Simulate shot with trip_timers (creates ~10us between each sensor) */
    trip_timers();
    delay(1);
    
    sensor_status = is_running();
    if ( sensor_status != 0x0F )
    {
      if ( DLT(DLT_CRITICAL) )
      {
        Serial.print(T("\r\nFailed: Not all sensors triggered: 0x")); Serial.print(sensor_status, HEX);
      }
      test_passed = false;
    }

    read_timers(&timer_counts[0]);
    
    /* Verify timestamps are non-zero and within reasonable range */
    for (j = N; j <= W; j++)
    {
      if ( timer_counts[j] == 0 || timer_counts[j] >= 0xFFFF )
      {
        if ( DLT(DLT_CRITICAL) )
        {
          Serial.print(T("\r\nFailed: Bad timestamp for ")); Serial.print(nesw[j]);
          Serial.print(T(": ")); Serial.print(timer_counts[j]);
        }
        test_passed = false;
      }
    }
    
    /* Verify all timestamps are close together (simulated shot) */
    int max_diff = 0;
    for (j = N; j <= W; j++)
    {
      int diff = abs((int)timer_counts[j] - (int)timer_counts[N]);
      if (diff > max_diff) max_diff = diff;
    }
    if (max_diff > 200)  /* Should be ~30us apart for simulated shot */
    {
      if ( DLT(DLT_CRITICAL) )
      {
        Serial.print(T("\r\nFailed: Timestamps too spread: ")); Serial.print(max_diff); Serial.print(T("us"));
      }
      test_passed = false;
    }
  }
  
  stop_timers();
  set_LED(L('.', '.', '.'));
  return test_passed;
}

void POST_trip_point(void)
{
  set_trip_point(20);
  set_LED(LED_RESET);
  return;
}

void set_trip_point(int pass_count)
{
  bool stay_forever;
  
  Serial.print(T("Setting trip point. Type ! or cycle power to exit\r\n"));
  
  stay_forever = (pass_count == 0);
  arm_timers();
  enable_face_interrupt();
  disable_timer_interrupt();
  face_strike = 0;

  json_vset_PWM = 128;
  set_vset_PWM(json_vset_PWM);

  while ( stay_forever || pass_count != 0 )
  {
    if ( Serial.read() == '!' )
    {
      Serial.print(T("\r\nExiting calibration\r\n"));
      enable_timer_interrupt();
      return;
    }

    show_sensor_status(is_running(), 0);
    Serial.print(T("\n\r"));
    
    if ( stay_forever )
    {
      if ( (is_running() == 0x0f) && (face_strike != 0) )
      {
        stop_timers();
        arm_timers();
        enable_face_interrupt();
        face_strike = 0;
      }
    }
    else
    {
      if ( pass_count != 0 )
      {
        pass_count--;
        if ( pass_count == 0 )
        {
          enable_timer_interrupt();
          return;
        }
      }
    }
    delay(ONE_SECOND / 5);
  }

  enable_timer_interrupt();
  return;
}

/*----------------------------------------------------------------
 * show_analog - O'scope display
 *--------------------------------------------------------------*/
unsigned int channel[] = {NORTH_ANA, EAST_ANA, SOUTH_ANA, WEST_ANA};
unsigned int max_input[4];
#define FULL_SCALE_SCOPE   128
#define SCALE        128/128
#define SAMPLE_TIME  (500000U)

void show_analog(int v)
{
  unsigned int i, sample;
  char o_scope[FULL_SCALE_SCOPE];
  unsigned long now;
  
  for (i = 0; i < FULL_SCALE_SCOPE; i++) o_scope[i] = ' ';
  o_scope[FULL_SCALE_SCOPE - 1] = 0;

  i = (read_reference() * SCALE) >> 2;
  if (i < FULL_SCALE_SCOPE) o_scope[i] = '|';

  max_input[N] = 0; max_input[E] = 0;
  max_input[S] = 0; max_input[W] = 0;
  now = micros();
  while ((micros() - now) <= SAMPLE_TIME)
  { 
    for (i = N; i <= W; i++)
    {
      sample = (ads1115_read_sensor(channel[i]) * SCALE) >> 2;
      if ( sample >= FULL_SCALE_SCOPE - 1 ) sample = FULL_SCALE_SCOPE - 2;
      if ( sample > max_input[i] ) max_input[i] = sample;
    }
  }

  for (i = N; i <= W; i++)
  {
    o_scope[max_input[i]] = nesw[i];
  }
  
  Serial.print(T("{\"OSCOPE\": ")); Serial.print(o_scope); Serial.print(T("\"}\r\n"));
  return;
}


/*----------------------------------------------------------------
 * show_sensor_status
 *--------------------------------------------------------------*/
void show_sensor_status(unsigned int sensor_status, shot_record_t* shot)
{
  unsigned int i;
  
  Serial.print(T(" PCNT:"));
  for (i = N; i <= W; i++)
  {
    if ( sensor_status & (1 << i) ) Serial.print(nesw[i]);
    else Serial.print(T("."));
  }

  if ( shot != 0 )
  {
    Serial.print(" Timers(us):");
    for (i = N; i <= W; i++)
    {
      Serial.print(T(" ")); Serial.print(nesw[i]); Serial.print(T(":")); Serial.print(shot->timer_count[i]); 
    }
  }
  
  Serial.print(T("  Face Strike:")); Serial.print(face_strike);
  Serial.print(T("  V_Ref:")); Serial.print(TO_VOLTS(read_reference()));
  Serial.print(T("  Temperature:")); Serial.print(temperature_C());
  Serial.print(T("  WiFi: Built-in"));

  return;
}

/*----------------------------------------------------------------
 * do_dlt - Diagnostics Log and Trace
 *--------------------------------------------------------------*/
bool do_dlt(unsigned int level)
{ 
  char s[20], str[20];

  if ((level & (is_trace | DLT_CRITICAL)) == 0)
  {
    return false;
  }

  dtostrf(micros() / 1000000.0, 7, 6, str);
  sprintf(s, "\n\r%s: ", str);
  Serial.print(s);

  return true;
}

/*----------------------------------------------------------------
 * unit_test, sample_calculations
 *--------------------------------------------------------------*/
static void unit_test(unsigned int mode)
{
  unsigned int i;
  unsigned int location;
  unsigned int shot_num;
  
  init_sensors();
  shot_num = 1;
  for (i = 0; i < TEST_SAMPLES; i++)
  {
    if ( sample_calculations(mode, i) )
    {
      location = compute_hit(&record[0]);
      record[0].shot_number = shot_num++;
      send_score(&record[0]);
      delay(ONE_SECOND / 2);
    }
    if ( mode == T_ONCE ) break;
  }
  return;
}

static bool sample_calculations(unsigned int mode, unsigned int sample)
{
  double x, y, angle, radius, polar, grid_step;
  int ix, iy;
  shot_record_t shot;
  
  switch (mode)
  {
    case T_ONCE:
      angle = 0;
      radius = json_sensor_dia / sqrt(2.0d) / 2.0d;
      x = radius * cos(angle);
      y = radius * sin(angle);
      record[0].timer_count[N] = RX(N, x, y);
      record[0].timer_count[E] = RX(E, x, y);
      record[0].timer_count[S] = RX(S, x, y);
      record[0].timer_count[W] = RX(W, x, y);
      record[0].timer_count[W] -= 20;  /* Rev 6.1: ~20us offset (was 200 @8MHz) */
      break;

    default:
    case T_SPIRAL:
      angle = (PI / 4.0) / 5.0 * ((double)sample);
      radius = 0.99d * (json_sensor_dia / 2.0) / sqrt(2.0d) * (double)sample / TEST_SAMPLES;
      x = radius * cos(angle);
      y = radius * sin(angle);
      record[0].timer_count[N] = RX(N, x, y);
      record[0].timer_count[E] = RX(E, x, y);
      record[0].timer_count[S] = RX(S, x, y);
      record[0].timer_count[W] = RX(W, x, y);
      break;

    case T_GRID:
      radius = 0.99d * (json_sensor_dia / 2.0d / sqrt(2.0d));
      grid_step = radius * 2.0d / (double)GRID_SIDE;
      ix = -GRID_SIDE / 2 + (sample % GRID_SIDE);
      iy = GRID_SIDE / 2 - (sample / GRID_SIDE);
      x = (double)ix * grid_step;
      y = (double)iy * grid_step;
      polar = sqrt(sq(x) + sq(y));
      angle = atan2(y, x) - (PI * json_sensor_angle / 180.0d);
      x = polar * cos(angle);
      y = polar * sin(angle);
      if ( sqrt(sq(x) + sq(y)) > radius ) return false;
      record[0].timer_count[N] = RX(N, x, y);
      record[0].timer_count[E] = RX(E, x, y);
      record[0].timer_count[S] = RX(S, x, y);
      record[0].timer_count[W] = RX(W, x, y);
      break;
  }
  return true;
}
