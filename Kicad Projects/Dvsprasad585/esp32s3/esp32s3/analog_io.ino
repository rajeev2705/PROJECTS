/*-------------------------------------------------------
 * 
 * analog_io.ino
 * 
 * General purpose Analog driver
 * VSKTarget - ESP32-S3 Port (Rev 6.1)
 * 
 * Rev 6.1: PCNT timing, no external logic ICs
 * - LED PWM via ESP32-S3 LEDC (true hardware PWM)
 * - VREF via LEDC PWM + RC filter (sent to daughter boards via FRC)
 * - Board revision returns REV_610
 * - I2C on dedicated GPIO1/2 (no pin sharing)
 * - Power: A0505XT-1WR3 isolated +/-5V for sensor daughter boards
 * 
 * ----------------------------------------------------*/

#include "vskETarget.h"
#include <Wire.h>

void set_vset_PWM(unsigned int pwm);

/*----------------------------------------------------------------
 * ADS1115 I2C ADC driver functions (unchanged from Rev 5.0)
 *--------------------------------------------------------------*/
#define ADS_REG_CONVERSION  0x00
#define ADS_REG_CONFIG      0x01

#define ADS_OS_START        0x8000
#define ADS_MUX_AIN0        0x4000
#define ADS_MUX_AIN1        0x5000
#define ADS_MUX_AIN2        0x6000
#define ADS_MUX_AIN3        0x7000
#define ADS_PGA_4096        0x0200
#define ADS_MODE_SINGLE     0x0100
#define ADS_DR_128SPS       0x0080
#define ADS_COMP_DISABLE    0x0003

static const uint16_t ads_mux[] = { ADS_MUX_AIN0, ADS_MUX_AIN1, ADS_MUX_AIN2, ADS_MUX_AIN3 };

int16_t ads1115_read(uint8_t i2c_addr, uint8_t channel)
{
  uint16_t config;
  int16_t result;
  
  if (channel > 3) channel = 0;
  config = ADS_OS_START | ads_mux[channel] | ADS_PGA_4096 | ADS_MODE_SINGLE | ADS_DR_128SPS | ADS_COMP_DISABLE;
  
  Wire.beginTransmission(i2c_addr);
  Wire.write(ADS_REG_CONFIG);
  Wire.write((uint8_t)(config >> 8));
  Wire.write((uint8_t)(config & 0xFF));
  Wire.endTransmission();
  
  delay(10);
  
  Wire.beginTransmission(i2c_addr);
  Wire.write(ADS_REG_CONVERSION);
  Wire.endTransmission();
  
  Wire.requestFrom(i2c_addr, (uint8_t)2);
  result = Wire.read() << 8;
  result |= Wire.read();
  
  return result;
}

int16_t ads1115_read_sensor(uint8_t channel)
{
  return ads1115_read(ADS1115_SENSOR_ADDR, channel);
}

int16_t ads1115_read_ref(uint8_t channel)
{
  return ads1115_read(ADS1115_REF_ADDR, channel);
}

/*----------------------------------------------------------------
 * function: init_analog_io
 * brief:    Initialize analog I/O (ADS1115 + I2C on dedicated pins)
 *--------------------------------------------------------------*/
void init_analog_io(void)
{
  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("init_analog_io() - ESP32-S3 dedicated I2C"));
  }
  
  /* I2C on dedicated GPIO1(SDA), GPIO2(SCL) - no pin sharing! */
  Wire.begin(I2C_SDA, I2C_SCL);

  Wire.beginTransmission(ADS1115_SENSOR_ADDR);
  if (Wire.endTransmission() != 0)
  {
    Serial.print(T("\r\n*** WARNING: ADS1115 Sensor ADC (0x48) not found! ***\r\n"));
  }
  
  Wire.beginTransmission(ADS1115_REF_ADDR);
  if (Wire.endTransmission() != 0)
  {
    Serial.print(T("\r\n*** WARNING: ADS1115 Reference ADC (0x49) not found! ***\r\n"));
  }

  Preferences prefs;
  prefs.begin("nonvol", true);
  json_vset_PWM = prefs.getInt("vset_pwm", 0);
  prefs.end();
  
  set_vset_PWM(json_vset_PWM);

  return;
}

/*----------------------------------------------------------------
 * function: set_LED_PWM_now
 * brief:    Program the PWM value immediately
 *           ESP32-S3: True hardware PWM via LEDC (not MCP23017 on/off!)
 *--------------------------------------------------------------*/
static unsigned int old_LED_percent = 0;

void set_LED_PWM_now
  (
  int new_LED_percent
  )
{
  if ( new_LED_percent == old_LED_percent )
  {
    return;
  }
  
  if ( DLT(DLT_DIAG) )
  {
    Serial.print(T("new_LED_percent:")); Serial.print(new_LED_percent); Serial.print(T("  old_LED_percent:")); Serial.print(old_LED_percent);
  }

  old_LED_percent = new_LED_percent;
  
  /* ESP32-S3: True PWM via LEDC - smooth dimming! */
  unsigned int pwm_value = (old_LED_percent * 255) / 100;
  ledcWrite(LED_PWM_CHANNEL, pwm_value);
  
  return;
}
  

/*----------------------------------------------------------------
 * function: set_LED_PWM
 * brief:    Program the PWM value with theatre-style ramping
 *           ESP32-S3: Smooth ramping possible via LEDC!
 *--------------------------------------------------------------*/
void set_LED_PWM
  (
  int new_LED_percent
  )
{
  int step;
  unsigned int pwm_value;
  
  if ( DLT(DLT_DIAG) )
  {
    Serial.print(T("new_LED_percent:")); Serial.print(new_LED_percent); Serial.print(T("  old_LED_percent:")); Serial.print(old_LED_percent);
  }

  if (new_LED_percent == LED_PWM_TOGGLE)
  {
    new_LED_percent = 0;
    if ( old_LED_percent == 0 )
    {
      new_LED_percent = json_LED_PWM;
    }
  }
  
  /* ESP32-S3: Smooth theatre-style ramping via LEDC PWM */
  step = (new_LED_percent > old_LED_percent) ? 1 : -1;
  while (old_LED_percent != new_LED_percent)
  {
    old_LED_percent += step;
    pwm_value = (old_LED_percent * 255) / 100;
    ledcWrite(LED_PWM_CHANNEL, pwm_value);
    delay(10);  // ~10ms per step for smooth ramp
  }

  return;
}


/*----------------------------------------------------------------
 * function: read_reference
 * brief:    Return the reference voltage via ADS1115 #2
 *--------------------------------------------------------------*/
unsigned int read_reference(void)
{
  int16_t raw = ads1115_read_ref(V_REFERENCE);
  if (raw < 0) raw = 0;
  return (unsigned int)raw;
}

/*----------------------------------------------------------------
 * function: revision
 * brief:    Return the board revision
 *--------------------------------------------------------------*/
unsigned int revision(void)
{
  return REV_610;     // ESP32-S3 Rev 6.1 (PCNT, no external logic)
}

/*----------------------------------------------------------------
 * function: max_analog
 * brief:    Return the largest analog input from ADS1115 #1
 *--------------------------------------------------------------*/
uint16_t max_analog(void)
{
  int16_t return_value;
  int16_t reading;

  return_value = ads1115_read_sensor(NORTH_ANA);

  reading = ads1115_read_sensor(EAST_ANA);
  if ( reading > return_value ) 
    return_value = reading;

  reading = ads1115_read_sensor(SOUTH_ANA);
  if ( reading > return_value ) 
    return_value = reading;

  reading = ads1115_read_sensor(WEST_ANA);
  if ( reading > return_value ) 
    return_value = reading;

  return (return_value < 0) ? 0 : (uint16_t)return_value;
}

/*----------------------------------------------------------------
 * function: temperature_C
 * brief:    Read the LM75 temperature sensor
 *--------------------------------------------------------------*/
 #define RTD_SCALE      (0.5)

double temperature_C(void)
{
  double return_value;
  int raw;

  raw = 0xffff;
   
  Wire.beginTransmission(TEMP_IC);
  Wire.write(0),
  Wire.endTransmission();

  Wire.requestFrom(TEMP_IC, 2);
  raw = Wire.read();
  raw <<= 8;
  raw += Wire.read();
  raw >>= 7;
  
  if ( raw & 0x0100 )
  {
    raw |= 0xFF00;
  }

  return_value = (double)(raw) * RTD_SCALE;
  
#if (SAMPLE_CALCULATIONS)
  return_value = 23.0;
#endif
    
  return return_value;
}


/*----------------------------------------------------------------
 * function: set_vset_PWM
 * brief:    Set the VREF PWM value
 *           ESP32-S3: Direct LEDC PWM + external RC filter
 *           Replaces MCP4725 DAC or manual potentiometer
 *--------------------------------------------------------------*/
 void set_vset_PWM
  (
  unsigned int value
  )
{
  value &= MAX_PWM;
  ledcWrite(VSET_PWM_CHANNEL, value);
  return;
}

/*----------------------------------------------------------------
 * function: compute_vset_PWM
 * brief:    Use a control loop to determine PWM setting for VREF
 *--------------------------------------------------------------*/
 void compute_vset_PWM
  (
  double vset
  )
{
  int vref_raw;
  int vref_desired;
  int vref_error;
  unsigned int pwm;
  int cycle_count;
  char s[128];
  char svref[15], svset[15];

  vref_desired = (int)(json_vset * 32767.0 / 4.096);
  pwm = json_vset_PWM;
  set_vset_PWM(json_vset_PWM);
  
  cycle_count = 0;
  while ( 1 )
  {
    vref_raw = read_reference();
    vref_error = vref_desired - vref_raw;
    pwm -= vref_error / 4;
    if ( pwm > 255 )
    {
      pwm = 255;
    }
    dtostrf(json_vset, 4, 2, svset);
    dtostrf(TO_VOLTS(vref_raw), 4, 2, svref);
    sprintf(s, "\r\nDesired: %s VREF: %s as read %d wanted %d error %d PWM %d", svset, svref, vref_raw, vref_desired, vref_error, pwm);
    output_to_all(s);
    
    if ( abs(vref_error) <= 2 )
    {
      break;
    }

    set_vset_PWM(pwm);
    delay(2 * ONE_SECOND);
    cycle_count++;
    if ( cycle_count > 100 )
    {
      sprintf(s, "\r\nvset_PWM exceeded.  Set value using CAL function and try again");
      output_to_all(s);
      return;
    }
  }

  sprintf(s, "\r\nDone\r\n");
  output_to_all(s);
  json_vset_PWM = pwm;
  
  Preferences prefs;
  prefs.begin("nonvol", false);
  prefs.putInt("vset_pwm", json_vset_PWM);
  prefs.end();
  
  return;
}

/*----------------------------------------------------------------
 * function: show_analog
 * brief:    Display all analog values for diagnostics
 *--------------------------------------------------------------*/
void show_analog(int v)
{
  char s[128];
  char str_c[10];
  int16_t reading;
  
  sprintf(s, "\r\n--- Analog Readings (ADS1115 via dedicated I2C) ---\r\n");
  output_to_all(s);
  
  reading = ads1115_read_sensor(NORTH_ANA);
  dtostrf(TO_VOLTS(reading), 4, 3, str_c);
  sprintf(s, "North (FRC Pin 1): %s V  (raw: %d)\r\n", str_c, reading);
  output_to_all(s);
  
  reading = ads1115_read_sensor(EAST_ANA);
  dtostrf(TO_VOLTS(reading), 4, 3, str_c);
  sprintf(s, "East  (FRC Pin 2): %s V  (raw: %d)\r\n", str_c, reading);
  output_to_all(s);
  
  reading = ads1115_read_sensor(SOUTH_ANA);
  dtostrf(TO_VOLTS(reading), 4, 3, str_c);
  sprintf(s, "South (FRC Pin 3): %s V  (raw: %d)\r\n", str_c, reading);
  output_to_all(s);
  
  reading = ads1115_read_sensor(WEST_ANA);
  dtostrf(TO_VOLTS(reading), 4, 3, str_c);
  sprintf(s, "West  (FRC Pin 4): %s V  (raw: %d)\r\n", str_c, reading);
  output_to_all(s);
  
  reading = ads1115_read_ref(V_REFERENCE);
  dtostrf(TO_VOLTS(reading), 4, 3, str_c);
  sprintf(s, "V_REF (SW ctrl):   %s V  (raw: %d)\r\n", str_c, reading);
  output_to_all(s);
  
  reading = ads1115_read_ref(V_12_LED);
  dtostrf(TO_VOLTS(reading) * K_12, 4, 3, str_c);
  sprintf(s, "V_12_LED:          %s V  (raw: %d)\r\n", str_c, reading);
  output_to_all(s);
  
  return;
}
