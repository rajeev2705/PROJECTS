/*----------------------------------------------------------------
 *
 * Compute_hit.ino
 *
 * Determine the score
 * VSKTarget - ESP32-S3 Port (Rev 6.1)
 *
 * Rev 6.1 PCNT Changes:
 *   - Timer values are now MICROSECOND timestamps (not 8MHz counter ticks)
 *   - OSCILLATOR_MHZ replaced with TICKS_PER_US (1.0, since 1 tick = 1 us)
 *   - clock_to_mm = s_of_sound (mm/us) directly
 *   - Miss detection checks for 0xFFFF (PCNT no-trigger sentinel)
 *   - All geometry math unchanged (works in "ticks", now ticks=microseconds)
 *
 *---------------------------------------------------------------*/
#include "vskETarget.h"
#include "json.h"
#include <math.h>

static int location;

#define THRESHOLD (0.001)

#define PI_ON_4 (PI / 4.0d)
#define PI_ON_2 (PI / 2.0d)

#define R(x)  (((x)+location) % 4)    // Rotate the target by location points

/*
 * PCNT timing: 1 tick = 1 microsecond (from esp_timer_get_time)
 * s_of_sound is in mm/us, so:
 *   distance_mm = ticks_us * s_of_sound
 *   ticks_us    = distance_mm / s_of_sound
 */
#define TICKS_PER_US  1.0                 // 1 tick = 1 microsecond

/*
 * Miss sentinel value from gpio.ino read_timers()
 */
#define TIMER_MISS_VALUE  0xFFFF

/*
 *  Variables
 */
extern const char* which_one[4];
extern int json_clock[4];

static sensor_t sensor[4];

unsigned int  pellet_calibre;     // Time offset to compensate for pellet diameter

static void remap_target(double* x, double* y);
static void doppler_fade(shot_record_t* record, sensor_t sensor[]);
static int  adjust_clocks( shot_record_t* shot, sensor_t sensor[]);
static void target_geometry( shot_record_t* shot, sensor_t sensor[]);
static bool check_for_inside( shot_record_t* shot);

/*----------------------------------------------------------------
 *
 * function: init_sensors()
 *
 * brief: Setup the constants in the structure
 *        Rev 6.1: Positions converted to microseconds using s_of_sound
 *        (distance_mm / s_of_sound_mm_per_us = time_us)
 * 
 * return: Sensor array updated with current geometry
 *
 *--------------------------------------------------------------*/
void init_sensors(void)
{
  if ( DLT(DLT_CRITICAL) ) 
  {
    Serial.print(T("init_sensors() - PCNT microsecond timing"));
  }
  
  s_of_sound = speed_of_sound(temperature_C(), json_rh);
  
  /*
   * pellet_calibre: time in microseconds for sound to travel half the pellet diameter
   * calibre_x10 is in tenths of mm, so:
   *   pellet_calibre_us = (calibre_mm / 2) / s_of_sound
   */
  pellet_calibre = ((double)json_calibre_x10 / 10.0d / 2.0d) / s_of_sound * TICKS_PER_US;
  
  /*
   * Sensor positions in microseconds (distance_mm / speed_mm_per_us)
   * These are "ticks" where 1 tick = 1 microsecond
   */
  sensor[N].index = N;
  sensor[N].x_tick = json_north_x / s_of_sound * TICKS_PER_US;
  sensor[N].y_tick = (json_sensor_dia / 2.0d + json_north_y) / s_of_sound * TICKS_PER_US;
  sensor[N].xphys_mm = 0;
  sensor[N].yphys_mm = json_sensor_dia / 2.0;
  
  sensor[E].index = E;
  sensor[E].x_tick = (json_sensor_dia / 2.0d + json_east_x) / s_of_sound * TICKS_PER_US;
  sensor[E].y_tick = (0.0d + json_east_y) / s_of_sound * TICKS_PER_US;
  sensor[E].xphys_mm = json_sensor_dia / 2.0d;
  sensor[E].yphys_mm = 0;
  
  sensor[S].index = S;
  sensor[S].x_tick = (0.0d + json_south_x) / s_of_sound * TICKS_PER_US;
  sensor[S].y_tick = -(json_sensor_dia / 2.0d + json_south_y) / s_of_sound * TICKS_PER_US;
  sensor[S].xphys_mm = 0;
  sensor[S].yphys_mm = -(json_sensor_dia / 2.0d);
  
  sensor[W].index = W;
  sensor[W].x_tick = -(json_sensor_dia / 2.0d + json_west_x) / s_of_sound * TICKS_PER_US;
  sensor[W].y_tick = json_west_y / s_of_sound * TICKS_PER_US;
  sensor[W].xphys_mm = -(json_sensor_dia / 2.0d);
  sensor[W].yphys_mm = 0;

  return;
}

/*----------------------------------------------------------------
 *
 * function: compute_hit
 *
 * brief: Determine the location of the hit
 *        Rev 6.1: timer_count[] values are microseconds from PCNT
 *        0xFFFF = sensor didn't trigger (miss)
 * 
 * return: Sensor location used to recognize shot, or MISS
 *
 *--------------------------------------------------------------*/

unsigned int compute_hit
  (
  shot_record_t* shot
  )
{
  int           i, j, count;
  double        estimate;
  int           trigger_sensor;
  double        last_estimate, error;
  double        x_avg, y_avg;
  double        z_offset_clock;
  double        clock_to_mm;
  
  if ( DLT(DLT_DIAG) )
  {
    Serial.print(T("compute_hit() - PCNT timestamps")); 
  }

  /*
   * Check for miss: face strike or any sensor returning TIMER_MISS_VALUE
   */
  if (shot->face_strike != 0 ||
      shot->timer_count[N] >= TIMER_MISS_VALUE ||
      shot->timer_count[E] >= TIMER_MISS_VALUE ||
      shot->timer_count[S] >= TIMER_MISS_VALUE ||
      shot->timer_count[W] >= TIMER_MISS_VALUE)
  {
    if ( DLT(DLT_DIAG) )
    {
      Serial.print(T("Miss detected: F:")); Serial.print(shot->face_strike); 
      Serial.print(T(" N:")); Serial.print(shot->timer_count[N]);
      Serial.print(T(" E:")); Serial.print(shot->timer_count[E]);
      Serial.print(T(" S:")); Serial.print(shot->timer_count[S]);
      Serial.print(T(" W:")); Serial.print(shot->timer_count[W]);
    }
    return MISS;
  }

  /*
   * Find the sensor that triggered first (smallest timestamp = closest)
   */
  location = N;
  count = shot->timer_count[N];
  for (i=N; i <= W; i++ )
  {
    if ( count > shot->timer_count[i] )
    {
      location = i;
      count = shot->timer_count[location];
    }
  }
  
  init_sensors();
  
  /*
   * Rev 6.1: clock_to_mm converts microsecond ticks to millimeters
   * Since ticks ARE microseconds: clock_to_mm = s_of_sound (mm/us)
   */
  clock_to_mm = s_of_sound / TICKS_PER_US;    // mm per tick (= mm/us)
  z_offset_clock = (double)json_z_offset / s_of_sound * TICKS_PER_US;  // z_offset in ticks
  
  if ( DLT(DLT_DIAG) )
  { 
    for (i=N; i <= W; i++)
    {
      Serial.print(which_one[i]); Serial.print(shot->timer_count[i]); Serial.print(T("us ")); 
    }
    Serial.print(T("SoS:")); Serial.print(s_of_sound); Serial.print(T("mm/us "));
  }

  error = 999999;
  count = 0;
  estimate = (json_sensor_dia / 2.0d) / s_of_sound * TICKS_PER_US;  // Initial estimate in ticks
  
  while (error > THRESHOLD && count < 20)
  {
    doppler_fade(shot, sensor);
    trigger_sensor = adjust_clocks(shot, sensor);
    target_geometry(shot, sensor);
    
    x_avg = 0;
    y_avg = 0;
    last_estimate = estimate;

    for (i=N; i <= W; i++)
    {
      if ( find_xy_3D(&sensor[i], estimate, z_offset_clock) )
      {
        x_avg += sensor[i].xr_tick;
        y_avg += sensor[i].yr_tick;
      }
    }

    x_avg /= 4.0d;
    y_avg /= 4.0d;
    shot->xphys_mm = x_avg * clock_to_mm;
    shot->yphys_mm = y_avg * clock_to_mm;
    
    estimate = sqrt(sq(sensor[trigger_sensor].x_tick - x_avg) + sq(sensor[trigger_sensor].y_tick - y_avg));
    error = fabs(last_estimate - estimate);

    if ( DLT(DLT_DIAG) )
    {
      Serial.print(T("x_avg:"));  Serial.print(x_avg);   Serial.print(T("  y_avg:")); Serial.print(y_avg); Serial.print(T(" estimate:"));  Serial.print(estimate);  Serial.print(T(" error:")); Serial.print(error);
      Serial.print(T("  shot->xphys_mm:"));  Serial.print(shot->xphys_mm);   Serial.print(T("  shot->yphys_mm:")); Serial.print(shot->yphys_mm); 
      Serial.print("\r\n");
    }
    count++;
  }
  
  if ( check_for_inside(shot) == false )
  {
    return MISS;
  }
  return location;
}


/*----------------------------------------------------------------
 * doppler_fade, adjust_clocks, target_geometry, find_xy_3D,
 * check_for_inside, send_score, send_miss, remap_target, send_timer
 * 
 * These are pure math functions - work identically with microsecond
 * ticks as they did with 8MHz counter ticks. The unit system is
 * consistent: all "tick" values are microseconds, and clock_to_mm
 * converts back to physical millimeters.
 *--------------------------------------------------------------*/

#define DOPPLER_RATIO    100.0

static void doppler_fade(shot_record_t* shot, sensor_t sensor[])
{
  int i;
  double distance;
  double ratio;

  for (i=N; i <= W; i++)
  {
    distance = sqrt(sq(sensor[i].xphys_mm - shot->xphys_mm) + sq(sensor[i].yphys_mm - shot->yphys_mm));
    ratio = sq(distance / DOPPLER_RATIO);
    sensor[i].doppler = (int)((json_doppler * ratio) + 0.5);
  }
  return;
}

static int adjust_clocks(shot_record_t* shot, sensor_t sensor[])
{
  int i;
  int largest;
  int trigger_sensor;
  
  largest = 0;
  for (i=N; i <= W; i++)
  {
    sensor[i].count = shot->timer_count[i] + sensor[i].doppler;
    if ( sensor[i].count > largest )
    {
      largest = sensor[i].count;
      trigger_sensor = i;
    }
  }

  for (i=N; i <= W; i++)
  {
    sensor[i].count = largest - sensor[i].count;
  }

  return trigger_sensor;
}

static void target_geometry(shot_record_t* shot, sensor_t sensor[])
{
  int i;
  
  for (i=N; i <= W; i++)
  {
    sensor[i].b = sensor[i].count;
    sensor[i].c = sqrt(sq(sensor[(i) % 4].x_tick - sensor[(i+1) % 4].x_tick) + sq(sensor[(i) % 4].y_tick - sensor[(i+1) % 4].y_tick));
  }
  
  for (i=N; i <= W; i++)
  {
    sensor[i].a = sensor[(i+1) % 4].b;
  }
  
  return;
}


bool find_xy_3D(sensor_t* sensor, double estimate, double z_offset_clock)
{
  double ae, be;
  double rotation;
  double x;
  
  x = sq(sensor->a + estimate) - sq(z_offset_clock);
  if (x < 0) { x = 0; }
  ae = sqrt(x);

  x = sq(sensor->b + estimate) - sq(z_offset_clock);
  if (x < 0) { x = 0; }
  be = sqrt(x);

  if ((ae + be) < sensor->c)
  {
    sensor->angle_A = 0;
  }
  else
  {
    sensor->angle_A = acos((sq(ae) - sq(be) - sq(sensor->c)) / (-2.0d * be * sensor->c));
  }

  switch (sensor->index)
  {
    case (N): 
      rotation = PI_ON_2 - PI_ON_4 - sensor->angle_A;
      sensor->xr_tick = sensor->x_tick + ((be) * sin(rotation));
      sensor->yr_tick = sensor->y_tick - ((be) * cos(rotation));
      break;
      
    case (E): 
      rotation = sensor->angle_A - PI_ON_4;
      sensor->xr_tick = sensor->x_tick - ((be) * cos(rotation));
      sensor->yr_tick = sensor->y_tick + ((be) * sin(rotation));
      break;
      
    case (S): 
      rotation = sensor->angle_A + PI_ON_4;
      sensor->xr_tick = sensor->x_tick - ((be) * cos(rotation));
      sensor->yr_tick = sensor->y_tick + ((be) * sin(rotation));
      break;
      
    case (W): 
      rotation = PI_ON_2 - PI_ON_4 - sensor->angle_A;
      sensor->xr_tick = sensor->x_tick + ((be) * cos(rotation));
      sensor->yr_tick = sensor->y_tick + ((be) * sin(rotation));
      break;

    default:
      break;
  }

  return true;
}


static bool check_for_inside(shot_record_t* shot)
{
  double x, y;
  double radius;

  x = shot->xphys_mm;
  y = shot->yphys_mm;
  radius = sqrt(sq(x) + sq(y));
  if ( radius > (json_sensor_dia / 2.0) )
  {
    return false;
  }
  return true;
}

void send_score(shot_record_t* shot)
{
  double x, y;
  double real_x, real_y;
  double radius;
  double angle;
  unsigned int volts;
  char   str[256], str_c[10];
  
  x = shot->xphys_mm;
  y = shot->yphys_mm;
  radius = sqrt(sq(x) + sq(y));
  angle = atan2(shot->yphys_mm, shot->xphys_mm) / PI * 180.0d;

  angle += json_sensor_angle;
  x = radius * cos(PI * angle / 180.0d);
  y = radius * sin(PI * angle / 180.0d);
  real_x = x;
  real_y = y;
  remap_target(&x, &y);
  
  sprintf(str, "\r\n{");
  output_to_all(str);
  
#if ( S_SHOT )
  sprintf(str, "\"shot\":%d, \"miss\":0, \"name\":\"%s\"", shot->shot_number, namesensor[json_name_id]);
  output_to_all(str);
  dtostrf((float)shot->shot_time/(float)(ONE_SECOND), 2, 2, str_c);
  sprintf(str, ", \"time\":%s ", str_c);
  output_to_all(str);
#endif

#if ( S_XY )
  dtostrf(x, 2, 2, str_c);
  sprintf(str, ",\"x\":%s", str_c);
  output_to_all(str);
  dtostrf(y, 2, 2, str_c);
  sprintf(str, ", \"y\":%s ", str_c);
  output_to_all(str);
  
  if ( json_target_type > 1 )
  {
    dtostrf(real_x, 2, 2, str_c);
    sprintf(str, ", \"real_x\":%s ", str_c);
    output_to_all(str);
    dtostrf(real_y, 2, 2, str_c);
    sprintf(str, ", \"real_y\":%s ", str_c);
    output_to_all(str);
  }
#endif

#if ( S_TIMERS )
  /* Rev 6.1: Timer values are microseconds */
  sprintf(str, ", \"N\":%dus, \"E\":%dus, \"S\":%dus, \"W\":%dus ", 
    (int)sensor[N].count, (int)sensor[E].count, (int)sensor[S].count, (int)sensor[W].count);
  output_to_all(str);
#endif

#if ( S_MISC ) 
  volts = read_reference();
  dtostrf(TO_VOLTS(volts), 2, 2, str_c);
  sprintf(str, ", \"V_REF\":%s, ", str_c);
  output_to_all(str);
  dtostrf(temperature_C(), 2, 2, str_c);
  sprintf(str, ", \"T\":%s, ", str_c);
  output_to_all(str);
  sprintf(str, ", \"VERSION\":%s ", SOFTWARE_VERSION);
  output_to_all(str);
#endif

  sprintf(str, "}\r\n");
  output_to_all(str);
  
  return;
}

void send_miss(shot_record_t* shot)
{
  char str[256];
  char str_c[10];
  
  if ( json_send_miss == 0 )
  {
    return;
  }

  sprintf(str, "\r\n{");
  output_to_all(str);
  
#if ( S_SHOT )
  sprintf(str, "\"shot\":%d, \"miss\":1, \"name\":\"%s\"", shot->shot_number, namesensor[json_name_id]);
  output_to_all(str);
  dtostrf((float)shot->shot_time/(float)(ONE_SECOND), 2, 2, str_c);
  sprintf(str, ", \"time\":%s ", str_c);
  output_to_all(str);
#endif

#if ( S_XY )
  sprintf(str, ", \"x\":0, \"y\":0 ");
  output_to_all(str);
#endif

#if ( S_TIMERS )
  /* Rev 6.1: Timer values are microseconds */
  sprintf(str, ", \"N\":%dus, \"E\":%dus, \"S\":%dus, \"W\":%dus ", 
    (int)shot->timer_count[N], (int)shot->timer_count[E], (int)shot->timer_count[S], (int)shot->timer_count[W]);
  output_to_all(str);
  sprintf(str, ", \"face\":%d ", shot->face_strike);
  output_to_all(str);
#endif

  sprintf(str, "}\n\r");
  output_to_all(str);

  return;
}

/*----------------------------------------------------------------
 * remap_target - identical to Arduino version (pure math)
 *--------------------------------------------------------------*/
struct new_target { double x; double y; };
typedef new_target new_target_t;

#define LAST_BULL (-1000.0)
#define D5_74 (74/2)
new_target_t five_bull_air_rifle_74mm[] = { {-D5_74, D5_74}, {D5_74, D5_74}, {0,0}, {-D5_74, -D5_74}, {D5_74, -D5_74}, {LAST_BULL, LAST_BULL}};

#define D5_79 (79/2)
new_target_t five_bull_air_rifle_79mm[] = { {-D5_79, D5_79}, {D5_79, D5_79}, {0,0}, {-D5_79, -D5_79}, {D5_79, -D5_79}, {LAST_BULL, LAST_BULL}};

#define D12_H (191.0/2.0)
#define D12_V (195.0/3.0)
new_target_t twelve_bull_air_rifle[] = { {-D12_H, D12_V+D12_V/2}, {0, D12_V+D12_V/2}, {D12_H, D12_V+D12_V/2},
                                          {-D12_H, D12_V/2}, {0, D12_V/2}, {D12_H, D12_V/2},
                                          {-D12_H, -D12_V/2}, {0, -D12_V/2}, {D12_H, -D12_V/2},
                                          {-D12_H, -(D12_V+D12_V/2)}, {0, -(D12_V+D12_V/2)}, {D12_H, -(D12_V+D12_V/2)},
                                          {LAST_BULL, LAST_BULL}};

#define O12_H (144.0/2.0)
#define O12_V (190.0/3.0)
new_target_t orion_bull_air_rifle[] = { {-O12_H, O12_V+O12_V/2}, {0, O12_V+O12_V/2}, {O12_H, O12_V+O12_V/2},
                                         {-O12_H, O12_V/2}, {0, O12_V/2}, {O12_H, O12_V/2},
                                         {-O12_H, -O12_V/2}, {0, -O12_V/2}, {O12_H, -O12_V/2},
                                         {-O12_H, -(O12_V+O12_V/2)}, {0, -(O12_V+O12_V/2)}, {O12_H, -(O12_V+O12_V/2)},
                                         {LAST_BULL, LAST_BULL}};

#define DBB_H (238.0 / 3.0)
#define DBB_V (144.0 / 2.0)
new_target_t daisy_bb_rifle[] = { {-(DBB_H+DBB_H/2), DBB_V}, {-DBB_H/2, DBB_V}, {DBB_H/2, DBB_V}, {(DBB_H+DBB_H/2), DBB_V},
                                   {-(DBB_H+DBB_H/2), 0}, {-DBB_H/2, 0}, {DBB_H/2, 0}, {(DBB_H+DBB_H/2), 0},
                                   {-(DBB_H+DBB_H/2), -DBB_V}, {-DBB_H/2, -DBB_V}, {DBB_H/2, -DBB_V}, {(DBB_H+DBB_H/2), -DBB_V},
                                   {LAST_BULL, LAST_BULL}};

new_target_t* ptr_list[] = { 0, 0, 0, 0, five_bull_air_rifle_74mm, five_bull_air_rifle_79mm, 0, 0, 0, 0, 0, orion_bull_air_rifle, twelve_bull_air_rifle, daisy_bb_rifle, 0};

static void remap_target(double* x, double* y)
{
  double distance, closest;
  double dx, dy;
  int i;
  new_target_t* ptr;
  
  if ( (json_target_type <= 1) || ( json_target_type > (int)(sizeof(ptr_list)/sizeof(new_target_t*)) ) )
  {
    return;
  }
  ptr = ptr_list[json_target_type];
  if ( ptr == 0 ) return;
  
  closest = 100000.0;
  i=0;
  while ( ptr->x != LAST_BULL )
  {
    distance = sqrt(sq(ptr->x - *x) + sq(ptr->y - *y));
    if ( distance < closest )
    {
      closest = distance;
      dx = ptr->x;
      dy = ptr->y;
    }
    ptr++;
    i++;
  }

  *x = *x - dx;
  *y = *y - dy;
  return;
}

void send_timer(int sensor_status)
{
  int i;
  unsigned int timer_count[4];
  
  read_timers(&timer_count[0]);
  
  Serial.print(T("{\"timer\": \""));
  for (i=0; i < 4; i++)
  {
    if ( sensor_status & (1<<i) )
      Serial.print(nesw[i]);
    else
      Serial.print(T("."));
  }

  /* Rev 6.1: Values in microseconds */
  Serial.print(T("\", "));
  for (i=N; i <= W; i++)
  {
    Serial.print(T("\"")); Serial.print(nesw[i]); Serial.print(T("\":"));  Serial.print(timer_count[i]);  Serial.print(T("us, "));
  }

  Serial.print(T("\"V_REF\":"));   Serial.print(TO_VOLTS(read_reference()));  Serial.print(T(", "));
  Serial.print(T("\"Version\":")); Serial.print(SOFTWARE_VERSION);
  Serial.print(T("}\r\n"));      

  return;
}
