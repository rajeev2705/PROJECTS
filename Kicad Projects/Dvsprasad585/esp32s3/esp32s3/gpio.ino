/*-------------------------------------------------------
 * 
 * gpio.ino
 * 
 * General purpose GPIO driver
 * VSKTarget - ESP32-S3 Port (Rev 6.2)
 * 
 * FULL SIGNAL CHAIN (Rev 6.2 - matches freETarget v5.2):
 * - Filter network on main board (RF1-RF4 10K + CF1-CF4 10nF)
 * - 2x LM339 quad comparators on main board (vs VREF)
 * - 4x 74HC74 D flip-flop RUN latches on main board
 * - LM339 outputs → ESP32-S3 GPIO ISR (timing capture)
 * - 74HC74 Q outputs → ESP32-S3 GPIO (RUN status)
 * - LATCH_CLR (GPIO38) → 74HC74 /CLR (active low)
 *
 * Signal Path:
 *   Daughter Board: Piezo → OPA1641 → amplified analog → FRC
 *   Main Board FRC → RC Filter → LM339 (vs VREF) → 74HC74 latch
 *                                  ↓
 *                         ESP32-S3 GPIO ISR (timestamp)
 * 
 * I2C Bus (SDA=GPIO1, SCL=GPIO2) - DEDICATED:
 *   ADS1115 #1 (0x48) - Sensor analog monitoring
 *   ADS1115 #2 (0x49) - Reference/monitor inputs
 *   LM75 (0x4F)       - Temperature sensor
 * 
 * ----------------------------------------------------*/

#include "timer.h"
#include <Wire.h>

/*
 * Timing state - ISR captures timestamps from LM339 comparator outputs
 * Uses esp_timer_get_time() for 1us resolution timestamps
 */
static volatile unsigned long pcnt_timestamp[4];    // N,E,S,W timestamps (microseconds)
static volatile bool          pcnt_triggered[4];    // Which sensors have fired
static volatile unsigned long pcnt_arm_time;        // When timers were armed

/*
 * ISR handlers for comparator outputs (LM339 → GPIO 4/5/6/7)
 * These fire on FALLING edge (LM339 open-collector pulls low on trigger)
 */
void IRAM_ATTR sensor_north_ISR(void) {
  if (!pcnt_triggered[0]) {
    pcnt_timestamp[0] = (unsigned long)esp_timer_get_time();
    pcnt_triggered[0] = true;
  }
}
void IRAM_ATTR sensor_east_ISR(void) {
  if (!pcnt_triggered[1]) {
    pcnt_timestamp[1] = (unsigned long)esp_timer_get_time();
    pcnt_triggered[1] = true;
  }
}
void IRAM_ATTR sensor_south_ISR(void) {
  if (!pcnt_triggered[2]) {
    pcnt_timestamp[2] = (unsigned long)esp_timer_get_time();
    pcnt_triggered[2] = true;
  }
}
void IRAM_ATTR sensor_west_ISR(void) {
  if (!pcnt_triggered[3]) {
    pcnt_timestamp[3] = (unsigned long)esp_timer_get_time();
    pcnt_triggered[3] = true;
  }
}

/*
 * ESP32-S3 GPIO initialization table (Rev 6.2 - full signal chain)
 */
const GPIO init_table[] = {
  {COMP_NORTH,      INPUT_PULLUP,  0},   // LM339 output (open-collector, pull-up on board)
  {COMP_EAST,       INPUT_PULLUP,  0},
  {COMP_SOUTH,      INPUT_PULLUP,  0},
  {COMP_WEST,       INPUT_PULLUP,  0},

  {RUN_NORTH,       INPUT,         0},   // 74HC74 Q output (RUN latch)
  {RUN_EAST,        INPUT,         0},
  {RUN_SOUTH,       INPUT,         0},
  {RUN_WEST,        INPUT,         0},

  {LATCH_CLR,       OUTPUT,        1},   // 74HC74 /CLR (active low, start HIGH=not clearing)

  {LED_RDY,         OUTPUT,        0},   // LEDs - DIRECT GPIO
  {LED_X,           OUTPUT,        0},
  {LED_Y,           OUTPUT,        0},

  {PAPER,           OUTPUT,        0},   // Paper motor via 2N7002 MOSFET
  {FACE_SENSOR,     INPUT_PULLUP,  0},   // Face sensor with pull-up

  {RAPID_RED_PIN,   OUTPUT,        0},   // Rapid fire LEDs
  {RAPID_GREEN_PIN, OUTPUT,        0},

  {EOF_MARKER, EOF_MARKER, EOF_MARKER} };


void face_ISR(void);

static void sw_state(unsigned int action);
static void send_fake_score(void);

static unsigned int dip_mask;
static char aux_spool[128];
static char json_spool[64];
static unsigned int aux_spool_in, aux_spool_out;
static unsigned int json_spool_in, json_spool_out;

/*-----------------------------------------------------
 * 
 * function: init_gpio
 * 
 * brief: Initialize ESP32-S3 GPIO for Rev 6.2 signal chain
 *        Sets up LM339/74HC74 interface pins
 * 
 *-----------------------------------------------------*/

void init_gpio(void)
{
  int i;

  if ( DLT(DLT_CRITICAL) ) 
  {
    Serial.print(T("init_gpio() - ESP32-S3 Rev 6.2 (LM339+74HC74)"));  
  }
  
  /* Initialize all ESP32-S3 GPIO pins */
  i = 0;
  while (init_table[i].port != EOF_MARKER )
  {
    pinMode(init_table[i].port, init_table[i].in_or_out);
    if ( init_table[i].in_or_out == OUTPUT )
    {
      digitalWrite(init_table[i].port, init_table[i].value);
    }
    i++;
  }

  /* Setup LEDC for LED PWM */
  ledcSetup(LED_PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  
  /* Setup PWM for software VREF control */
  ledcSetup(VSET_PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(VREF_DAC_PIN, VSET_PWM_CHANNEL);
  
  /* Initialize timing system */
  init_pcnt();
  
  multifunction_init();
  disable_face_interrupt();
  set_LED_PWM(0);
  
  digitalWrite(PAPER, 0);
  
  /* Clear all 74HC74 latches on startup */
  digitalWrite(LATCH_CLR, LOW);
  delayMicroseconds(10);
  digitalWrite(LATCH_CLR, HIGH);
  
  return;
}

/*-----------------------------------------------------
 * 
 * function: init_pcnt
 * 
 * brief: Initialize timing system
 *        Clears timestamps and trigger flags
 * 
 *-----------------------------------------------------*/
void init_pcnt(void)
{
  for (int i = 0; i < 4; i++) {
    pcnt_timestamp[i] = 0;
    pcnt_triggered[i] = false;
  }
  pcnt_arm_time = 0;

  if ( DLT(DLT_INFO) )
  {
    Serial.print(T("\r\nTiming: LM339 → GPIO ISR edge capture"));
    Serial.print(T("\r\nTiming: N=GPIO4 E=GPIO5 S=GPIO6 W=GPIO7"));
    Serial.print(T("\r\nLatch:  RUN N=GPIO15 E=16 S=17 W=18"));
    Serial.print(T("\r\nLatch:  CLR=GPIO38 (active low)"));
    Serial.print(T("\r\nResolution = 1us (esp_timer)"));
  }
}

/*-----------------------------------------------------
 * 
 * function: set_vref
 * 
 * brief: Set VREF voltage via PWM + RC filter (R7+C9)
 *        Drives LM339 inverting inputs (comparator threshold)
 *        Also sent to FRC pin 9 for daughter board reference
 * 
 *-----------------------------------------------------*/
void set_vref(unsigned int millivolts)
{
  unsigned int pwm_value;
  
  if (millivolts > 3300) millivolts = 3300;
  pwm_value = (unsigned int)((millivolts * 255UL) / 3300UL);
  ledcWrite(VSET_PWM_CHANNEL, pwm_value);
}

/*-----------------------------------------------------
 * 
 * function: is_running
 * 
 * brief: Read 74HC74 RUN latch outputs to determine
 *        which sensors have been triggered
 * 
 * return: Bitmask of triggered sensors
 * 
 *-----------------------------------------------------*/

unsigned int is_running(void)
{
  unsigned int result = 0;
  
  if (digitalRead(RUN_NORTH)) result |= TRIP_NORTH;
  if (digitalRead(RUN_EAST))  result |= TRIP_EAST;
  if (digitalRead(RUN_SOUTH)) result |= TRIP_SOUTH;
  if (digitalRead(RUN_WEST))  result |= TRIP_WEST;
  
  return result;
}

/*-----------------------------------------------------
 * 
 * function: arm_timers
 * 
 * brief: Prepare for a new shot cycle
 *        1. Clear 74HC74 latches (pulse LATCH_CLR low)
 *        2. Clear ISR timestamps
 *        3. Attach ISRs to comparator outputs
 *
 *-----------------------------------------------------*/
void arm_timers(void)
{
  /* Clear all 74HC74 RUN latches */
  digitalWrite(LATCH_CLR, LOW);
  delayMicroseconds(1);
  digitalWrite(LATCH_CLR, HIGH);
  
  /* Clear all trigger flags and timestamps */
  for (int i = 0; i < 4; i++) {
    pcnt_timestamp[i] = 0;
    pcnt_triggered[i] = false;
  }
  
  /* Record the arm time for relative timestamp calculation */
  pcnt_arm_time = (unsigned long)esp_timer_get_time();
  
  /* Attach falling-edge ISR to each comparator output
   * LM339 open-collector: idle HIGH (pull-up), trigger LOW */
  attachInterrupt(digitalPinToInterrupt(COMP_NORTH), sensor_north_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(COMP_EAST),  sensor_east_ISR,  FALLING);
  attachInterrupt(digitalPinToInterrupt(COMP_SOUTH), sensor_south_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(COMP_WEST),  sensor_west_ISR,  FALLING);
  
  return;
}

void clear_running(void)
{
  /* Clear 74HC74 latches */
  digitalWrite(LATCH_CLR, LOW);
  delayMicroseconds(1);
  digitalWrite(LATCH_CLR, HIGH);
  
  for (int i = 0; i < 4; i++) {
    pcnt_triggered[i] = false;
    pcnt_timestamp[i] = 0;
  }
  return;
}

void stop_timers(void)
{
  detachInterrupt(digitalPinToInterrupt(COMP_NORTH));
  detachInterrupt(digitalPinToInterrupt(COMP_EAST));
  detachInterrupt(digitalPinToInterrupt(COMP_SOUTH));
  detachInterrupt(digitalPinToInterrupt(COMP_WEST));
  return;
}

void trip_timers(void)
{
  /* Self-test: simulate all 4 sensor triggers with small delays */
  unsigned long now = (unsigned long)esp_timer_get_time();
  pcnt_timestamp[0] = now;
  pcnt_triggered[0] = true;
  delayMicroseconds(10);
  pcnt_timestamp[1] = (unsigned long)esp_timer_get_time();
  pcnt_triggered[1] = true;
  delayMicroseconds(10);
  pcnt_timestamp[2] = (unsigned long)esp_timer_get_time();
  pcnt_triggered[2] = true;
  delayMicroseconds(10);
  pcnt_timestamp[3] = (unsigned long)esp_timer_get_time();
  pcnt_triggered[3] = true;
  return;
}

/*-----------------------------------------------------
 * Sensor interrupt enable/disable
 *-----------------------------------------------------*/
void enable_sensor_interrupt(void)
{
  arm_timers();
}

void disable_sensor_interrupt(void)
{
  stop_timers();
}

/*-----------------------------------------------------
 * Face interrupt - DIRECT GPIO on ESP32-S3
 *-----------------------------------------------------*/
void enable_face_interrupt(void)
{
  attachInterrupt(digitalPinToInterrupt(FACE_SENSOR), face_ISR, FALLING);
  return;
}

void disable_face_interrupt(void)
{
  detachInterrupt(digitalPinToInterrupt(FACE_SENSOR));
  return;
}

/*-----------------------------------------------------
 * 
 * function: read_DIP
 * 
 * brief: READ the DIP switch setting (JSON controlled)
 * 
 *-----------------------------------------------------*/
unsigned int read_DIP(unsigned int dip_mask)
{
  unsigned int return_value;
  
  return_value = json_dip_switch;
  return_value |= 0xF0;

  return return_value;
}  

/*-----------------------------------------------------
 * 
 * function: set_LED
 * 
 * brief:    Set the state of all the LEDs
 *           DIRECT GPIO - no I2C latency!
 * 
 *-----------------------------------------------------*/
void set_LED(int state_RDY, int state_X, int state_Y)
{ 
  switch (state_RDY)
  {
    case 0: case '.': digitalWrite(LED_RDY, 0); break;
    case 1: case '*': digitalWrite(LED_RDY, 1); break;
  }
  
  switch (state_X)
  {
    case 0: case '.': digitalWrite(LED_X, 0); break;
    case 1: case '*': digitalWrite(LED_X, 1); break;
  }

  switch (state_Y)
  {
    case 0: case '.': digitalWrite(LED_Y, 0); break;
    case 1: case '*': digitalWrite(LED_Y, 1); break;
  }
  return;  
}

bool read_in(unsigned int port)
{
  return digitalRead(port);
}

/*-----------------------------------------------------
 * 
 * function: read_timers
 * 
 * brief: Read ISR timestamps and convert to counter values
 *        Returns relative timestamps from arm_time in us
 * 
 *-----------------------------------------------------*/
void read_timers(unsigned int* timer_ptr)
{
  for (int i = N; i <= W; i++)
  {
    if (pcnt_triggered[i]) {
      *(timer_ptr + i) = (unsigned int)(pcnt_timestamp[i] - pcnt_arm_time);
    } else {
      *(timer_ptr + i) = 0xFFFF;
    }
  }
  return;
}

/*-----------------------------------------------------
 * drive_paper, paper_on_off - via 2N7002 MOSFET
 *-----------------------------------------------------*/
void drive_paper(void)
{
  unsigned long s_time, s_count;
  volatile unsigned long paper_time;

  s_time = json_paper_time;
  if ( json_step_time != 0 )
  {
    s_time = json_step_time;
  }

  s_count = 1;
  if ( json_step_count != 0 )
  {
    s_count = json_step_count;
  }

  if ( s_time == 0 ) return;

  timer_new(&paper_time, s_time);

  while ( s_count )
  {
    paper_on_off(true);
    paper_time = s_time; 
    while ( paper_time != 0 ) { continue; }
    paper_on_off(false);
    paper_time = 5;
    while ( paper_time ) { continue; }
    s_count--;
  }

  timer_delete(&paper_time);
  return;
}

void paper_on_off(bool on)
{
  digitalWrite(PAPER, on ? PAPER_ON : PAPER_OFF);
  return;
}

/*-----------------------------------------------------
 * Face ISR, blink_fault
 *-----------------------------------------------------*/
void IRAM_ATTR face_ISR(void)
{
  face_strike++;
}

void blink_fault(unsigned int fault_code)
{
  unsigned int i;
  for (i = 0; i < 3; i++)
  {
    set_LED(fault_code & 4, fault_code & 2, fault_code & 1);
    delay(ONE_SECOND / 4);
    fault_code = ~fault_code;
    set_LED(fault_code & 4, fault_code & 2, fault_code & 1);
    delay(ONE_SECOND / 4);
    fault_code = ~fault_code;
  }
  return;
}

/*-----------------------------------------------------
 * Multifunction switches - JSON controlled
 *-----------------------------------------------------*/
void multifunction_init(void)
{
  return;
}

void multifunction_switch(void)
{
  return;
}

void multifunction_wait_open(void)
{
  return;
}

void multifunction_display(void)
{
  char s[128];
  sprintf(s, "\"MFS\": \"JSON controlled\",\n\r");
  output_to_all(s);
  return;
}

/*-----------------------------------------------------
 * Serial port spooling and I/O functions
 *-----------------------------------------------------*/
char get_all(void) 
{
  char ch = 0;

  if ( Serial.available() ) { ch = Serial.read(); }
  else if ( Serial1.available() ) { ch = Serial1.read(); }

  return ch;
}

void output_to_all(char* s) 
{
  Serial.print(s);
  Serial1.print(s);
  return;
}

void char_to_all(char ch) 
{
  Serial.print(ch);
  Serial1.print(ch);
  return;
}

void rapid_green(unsigned int state)
{
  digitalWrite(RAPID_GREEN_PIN, state);
  return;
}

void rapid_red(unsigned int state)
{
  digitalWrite(RAPID_RED_PIN, state);
  return;
}

/*-----------------------------------------------------
 * AUX Spool
 *-----------------------------------------------------*/
char aux_spool_read(void) {
  char ch = aux_spool[aux_spool_out];
  aux_spool_out = (aux_spool_out + 1) % sizeof(aux_spool);
  return ch;
}

int aux_spool_available(void) {
  return (aux_spool_in != aux_spool_out);
}

void aux_spool_put(char ch) {
  aux_spool[aux_spool_in] = ch;
  aux_spool_in = (aux_spool_in + 1) % sizeof(aux_spool);
}

/*-----------------------------------------------------
 * JSON Spool
 *-----------------------------------------------------*/
char json_spool_read(void) {
  char ch = json_spool[json_spool_out];
  json_spool_out = (json_spool_out + 1) % sizeof(json_spool);
  return ch;
}

int json_spool_available(void) {
  return (json_spool_in != json_spool_out);
}

void json_spool_put(char ch) {
  json_spool[json_spool_in] = ch;
  json_spool_in = (json_spool_in + 1) % sizeof(json_spool);
}

/*-----------------------------------------------------
 * digital_test - Hardware diagnostics
 *-----------------------------------------------------*/
void digital_test(void)
{
  char s[128];
  unsigned int run;
  
  sprintf(s, "\r\n--- Rev 6.2 Digital Test ---\r\n");
  output_to_all(s);
  
  /* Read RUN latches */
  run = is_running();
  sprintf(s, "RUN latches: N=%d E=%d S=%d W=%d (0x%02X)\r\n",
    (run & TRIP_NORTH) ? 1 : 0,
    (run & TRIP_EAST)  ? 1 : 0,
    (run & TRIP_SOUTH) ? 1 : 0,
    (run & TRIP_WEST)  ? 1 : 0,
    run);
  output_to_all(s);
  
  /* Read comparator outputs (should be HIGH when idle) */
  sprintf(s, "COMP pins: N=%d E=%d S=%d W=%d (expect 1111 idle)\r\n",
    digitalRead(COMP_NORTH),
    digitalRead(COMP_EAST),
    digitalRead(COMP_SOUTH),
    digitalRead(COMP_WEST));
  output_to_all(s);
  
  /* Test latch clear */
  sprintf(s, "Clearing latches...\r\n");
  output_to_all(s);
  clear_running();
  run = is_running();
  sprintf(s, "After clear: 0x%02X (expect 0x00)\r\n", run);
  output_to_all(s);
  
  /* Test LEDs */
  sprintf(s, "LED test: RDY-X-Y cycling...\r\n");
  output_to_all(s);
  set_LED(LON, LOF, LOF); delay(300);
  set_LED(LOF, LON, LOF); delay(300);
  set_LED(LOF, LOF, LON); delay(300);
  set_LED(LOF, LOF, LOF);
  
  sprintf(s, "--- Test Complete ---\r\n");
  output_to_all(s);
  
  return;
}
