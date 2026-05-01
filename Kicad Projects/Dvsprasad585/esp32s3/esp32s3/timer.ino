/*-------------------------------------------------------
 * 
 * timer.ino
 * 
 * Timer interrupt file
 * VSKTarget - ESP32-S3 Port (Rev 6.1)
 * 
 * Rev 6.1 Changes:
 * - ISR reads PCNT trigger flags instead of RUN_NORTH/EAST/SOUTH/WEST
 * - No 74HC74 flip-flops to read
 * - Sensor detection via pcnt_triggered[] flags set by GPIO ISRs
 * 
 * Uses ESP32 hw_timer_t API for 1KHz system tick
 * 
 * ----------------------------------------------------*/
#include "gpio.h"
#include "json.h"

/*
 * Definitions
 */
#define FREQUENCY 1000ul                        // 1000 Hz
#define N_TIMERS  12                            // Keep space for 12 timers
#define PORT_STATE_IDLE 0                       // There are no sensor inputs
#define PORT_STATE_WAIT 1                       // Some sensor inputs are present, but not all
#define PORT_STATE_DONE 2                       // All of the inputs are present

/*
 * Local Variables
 */
static unsigned long* timersensor[N_TIMERS];    // Active timer list
static unsigned long isr_timer;                 // Elapsed time counter
static unsigned int isr_state = PORT_STATE_IDLE;// Current acquisition state

static hw_timer_t* hw_timer = NULL;             // ESP32 hardware timer handle
static portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

/*-----------------------------------------------------
 * 
 * function: init_timer
 * 
 * brief: Initialize the timer channels using ESP32 hw_timer
 * 
 *-----------------------------------------------------*/
void init_timer(void)
{
  int i;
  
  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("init_timer() - Rev 6.1 PCNT"));
  }
  
  hw_timer = timerBegin(0, 80, true);           // Timer 0, prescaler 80, count up
  timerAttachInterrupt(hw_timer, &onTimer, true);
  timerAlarmWrite(hw_timer, 1000, true);         // 1ms = 1KHz, auto-reload
  timerAlarmDisable(hw_timer);

  for (i = 0; i < N_TIMERS; i++)
  {
    timersensor[i] = 0;
  }

  timer_new(&isr_timer, 0);

  return;
}

/*-----------------------------------------------------
 * enable/disable timer interrupt
 *-----------------------------------------------------*/

void enable_timer_interrupt(void)
{
  if (hw_timer != NULL)
  {
    timerAlarmEnable(hw_timer);
  }
  return;
}

void disable_timer_interrupt(void)
{
  if (hw_timer != NULL)
  {
    timerAlarmDisable(hw_timer);
  }
  return;
}


/*-----------------------------------------------------
 * 
 * function: onTimer (ISR)
 * 
 * brief: Timer interrupt handler for ESP32-S3
 *        Rev 6.1: Reads PCNT trigger flags instead of
 *        74HC74 flip-flop RUN latches
 * 
 *-----------------------------------------------------*/

void IRAM_ATTR onTimer()
{
  unsigned int pin;
  unsigned int i;

  portENTER_CRITICAL_ISR(&timerMux);

  /* Refresh the timers */
  for (i = 0; i < N_TIMERS; i++)
  {
    if ( (timersensor[i] != 0)
        && ( *timersensor[i] != 0 ) )
    {
      (*timersensor[i])--;
    }
  }
  
  /*
   * Rev 6.1: Read PCNT trigger flags instead of RUN latch GPIOs
   * is_running() now reads pcnt_triggered[] flags set by GPIO ISRs
   * in gpio.ino (sensor_north_ISR, etc.)
   */
  pin = is_running();

  /* Read the timer hardware based on the ISR state */
  switch (isr_state)
  {
    case PORT_STATE_IDLE:
      if ( pin != 0 )
      { 
        isr_timer = (unsigned long)((json_sensor_dia / 0.30) / 1000.0) + 1;
        isr_state = PORT_STATE_WAIT;
      }
      break;
          
    case PORT_STATE_WAIT:
      if ( (pin == 0x0F)
          || (isr_timer == 0) )
      {
        aquire();
        clear_running();
        isr_timer = json_min_ring_time;
        isr_state = PORT_STATE_DONE;
      }
      break;
      
    case PORT_STATE_DONE:
      if ( pin != 0 )
      {
        isr_timer = json_min_ring_time;
        clear_running();
      }
      else
      {
        if ( isr_timer == 0 )
        {
          arm_timers();
          isr_state = PORT_STATE_IDLE;
        }
      }
      break;
  }

  portEXIT_CRITICAL_ISR(&timerMux);

  return;
}


/*-----------------------------------------------------
 * timer_new / timer_delete
 *-----------------------------------------------------*/
unsigned int timer_new
(
  unsigned long* new_timer,
  unsigned long  start_time
)
{
  unsigned int i;

  portENTER_CRITICAL(&timerMux);
  
  for (i = 0; i < N_TIMERS; i++)
  {
    if ( (timersensor[i] == 0)
      || (timersensor[i] == new_timer) )
    {
      timersensor[i] = new_timer;
      *timersensor[i] = start_time;
      portEXIT_CRITICAL(&timerMux);
      return 1;
    }
  }

  portEXIT_CRITICAL(&timerMux);

  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("No space for timer"));
  }
  
  return 0;
}

unsigned int timer_delete
(
  unsigned long* old_timer
)
{
  unsigned int i;

  portENTER_CRITICAL(&timerMux);
  
  for (i = 0; i < N_TIMERS; i++)
  {
    if ( timersensor[i] == old_timer )
    {
      timersensor[i] = 0;
      portEXIT_CRITICAL(&timerMux);
      return 1;
    }
  }

  portEXIT_CRITICAL(&timerMux);
  return 0;
}
