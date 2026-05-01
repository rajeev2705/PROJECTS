/*----------------------------------------------------------------
 *
 * analog_io.h
 *
 * Header file for Analog IO functions
 * VSKTarget - ESP32-S3 Port (Rev 6.1)
 *
 *---------------------------------------------------------------*/
#ifndef _ANALOG_IO_H_
#define _ANALOG_IO_H_

/*
 * Global functions
 */
void init_analog_io(void);              // Setup the analog hardware
unsigned int read_reference(void);      // Read the feedback channel
void show_analog(int v);                // Display the analog values
double temperature_C(void);             // Temperature in degrees C
unsigned int revision(void);            // Return the board revision
void set_LED_PWM(int percent);          // Ramp the PWM duty cycle
void set_LED_PWM_now(int percent);      // Set the PWM duty cycle
void set_vset_PWM(unsigned int value);  // Value to write to PWM
void compute_vset_PWM(double value);    // Reference voltage control loop

/*
 *  Analog Input Strategy - ESP32-S3 (Rev 6.1)
 *  =============================================
 *  
 *  ESP32-S3 has no DAC and limited ADC accuracy.
 *  External ADS1115 16-bit I2C ADC modules used for all analog:
 *  
 *  ADS1115 #1 (0x48): Sensor analog monitoring (diagnostics only)
 *    - Ch A0: North sensor level
 *    - Ch A1: East sensor level
 *    - Ch A2: South sensor level
 *    - Ch A3: West sensor level
 *  
 *  ADS1115 #2 (0x49): Reference and power monitoring
 *    - Ch A0: V_REFERENCE feedback
 *    - Ch A1: V_12_LED monitor
 *  
 *  I2C Bus: SDA=GPIO1, SCL=GPIO2 (DEDICATED, not shared)
 *  Also on I2C: LM75 temperature sensor (0x4F)
 *  
 *  NOTE: Shot timing is NOT done via ADC. It uses PCNT
 *  (GPIO 4/5/6/7 digital pulse edge capture). ADS1115 readings
 *  are for diagnostics and VREF calibration only.
 */

/*
 *  Sensor Analog Channel Mapping (ADS1115 #1)
 */
#define NORTH_ANA    0       // ADS1115 #1, Channel A0
#define EAST_ANA     1       // ADS1115 #1, Channel A1
#define SOUTH_ANA    2       // ADS1115 #1, Channel A2
#define WEST_ANA     3       // ADS1115 #1, Channel A3

/*
 *  Reference/Monitor Channel Mapping (ADS1115 #2)
 */
#define V_REFERENCE  0       // ADS1115 #2, Channel A0
#define V_12_LED     1       // ADS1115 #2, Channel A1
#define K_12         ((10000.0d + 2200.0d) / 2200.0d) // Resistor divider

/*
 *  ADS1115 Configuration
 */
#define ADS_GAIN_TWOTHIRDS  0x0000  // +/-6.144V
#define ADS_GAIN_ONE        0x0200  // +/-4.096V
#define ADS_GAIN_TWO        0x0400  // +/-2.048V

#define ADS_SENSOR_GAIN     ADS_GAIN_ONE
#define ADS_REF_GAIN        ADS_GAIN_ONE

/*
 * ESP32-S3 uses LEDC for PWM
 */
#define MAX_ANALOG   0x7FFF    // ADS1115 16-bit signed positive range
#define MAX_PWM      0xFF      // PWM 8-bit resolution

#define TO_VOLTS(x) ( ((double)(x) * 4.096) / 32768.0 )

#define TEMP_IC      (0x9E >> 1)   // LM75 I2C address (0x4F)

#define LED_PWM_OFF     0x00
#define LED_PWM_TOGGLE  0xAB

#endif
