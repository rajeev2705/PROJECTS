/*-------------------------------------------------------
 * 
 * speed_of_sound.ino
 * 
 * Compute the speed of sound based on Temp & Humidity
 * VSKTarget - ESP32-S3 Port (Rev 6.0)
 * 
 * No hardware-specific changes - pure math
 * 
 * Contributed by Steve Carrington
 * 
 * ----------------------------------------------------*/

#include "vskETarget.h"
#include "diag_tools.h"

#define TO_MM 1000.0d
#define TO_US 1000000.0d
#define R 8314.46261815324d
#define M 28.966d
#define speed_MPSTo  331.38d

double speed_of_sound(double temperature, int relative_humidity)
{
  double speed_MPS;
  double speed_mmPuS;
  double y;
  double TK;
  double vapor_pressure;
  double mole_fraction;
  double specific_heat_ratio;
  double mean_molar_weight;
  double change_in_speed;

  if (temperature < 0.0d)
  {
    relative_humidity = 0;
  }

  TK = 273.15d + temperature;

  vapor_pressure = exp(((-7511.52d / TK) + 96.5389644d + (0.02399897d * (TK)) + (-0.000011654551d * sq(TK)) + (-0.000000012810336d * (TK * TK * TK)) + (0.000000000020998405d * (TK * TK * TK * TK))) - 12.150799d * log(TK));

  mole_fraction = 0.01d * ((double)relative_humidity) * vapor_pressure / 101325.0d;
  specific_heat_ratio = (7.0d + mole_fraction) / (5.0d + mole_fraction);
  mean_molar_weight = (M - (M - 18.01528d) * mole_fraction);
  change_in_speed = (1.0d / sqrt(1.4d / M) * 100.0d) * sqrt(specific_heat_ratio / mean_molar_weight) - 100.0d;
  y = 1.40092d - (0.0000196395d * temperature) - (0.000000162593d * sq(temperature));

  speed_MPS = sqrt((y * R * TK) / M)
              + ((speed_MPSTo / 100.0d) * change_in_speed);

  speed_mmPuS = speed_MPS * TO_MM / TO_US;
  
  if ( DLT(DLT_DIAG) )
  {
    Serial.print(T("Temperature: ")); Serial.print(temperature, 0); Serial.print(T("  Humidity: ")); Serial.print(relative_humidity); Serial.print(T("  Speed of Sound:  ")); Serial.println(speed_MPS, 2);
  }

  return speed_mmPuS;
}

void sound_test(void)
{
  int trace_memory;
  trace_memory = is_trace;
  is_trace = DLT_DIAG;
  
  speed_of_sound(-10.0,   0);
  speed_of_sound( 20.0,   0);
  speed_of_sound( 30.0,   0);
  speed_of_sound(-10.0, 100);
  speed_of_sound( 20.0, 100);
  speed_of_sound( 30.0, 100);

  is_trace = trace_memory;
  return;
}
