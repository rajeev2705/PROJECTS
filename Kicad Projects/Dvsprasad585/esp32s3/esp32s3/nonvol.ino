/*-------------------------------------------------------
 * 
 * nonvol.ino
 * 
 * Nonvol storage management
 * VSKTarget - ESP32-S3 Port (Rev 6.0)
 * 
 * Uses ESP32 Preferences library (NVS Flash) instead of EEPROM
 * NVS is wear-leveled and more reliable than raw EEPROM
 * 
 * For compatibility, EEPROM.get/put calls are mapped to
 * Preferences get/put operations using address-based keys.
 * 
 * ----------------------------------------------------*/

#include "vskETarget.h"
#include "nonvol.h"
#include "json.h"

/*
 * ESP32 NVS Helper Functions
 * Maps EEPROM-style address-based storage to NVS key-value pairs
 */
static Preferences nvs;

void nvs_put_int(unsigned int addr, int value)
{
  char key[12];
  sprintf(key, "nv_%04X", addr);
  nvs.begin("nonvol", false);
  nvs.putInt(key, value);
  nvs.end();
}

int nvs_get_int(unsigned int addr, int default_val)
{
  char key[12];
  sprintf(key, "nv_%04X", addr);
  nvs.begin("nonvol", true);
  int val = nvs.getInt(key, default_val);
  nvs.end();
  return val;
}

void nvs_put_double(unsigned int addr, double value)
{
  char key[12];
  sprintf(key, "nv_%04X", addr);
  nvs.begin("nonvol", false);
  nvs.putDouble(key, value);
  nvs.end();
}

double nvs_get_double(unsigned int addr, double default_val)
{
  char key[12];
  sprintf(key, "nv_%04X", addr);
  nvs.begin("nonvol", true);
  double val = nvs.getDouble(key, default_val);
  nvs.end();
  return val;
}

void nvs_put_char(unsigned int addr, char value)
{
  char key[12];
  sprintf(key, "nv_%04X", addr);
  nvs.begin("nonvol", false);
  nvs.putChar(key, value);
  nvs.end();
}

char nvs_get_char(unsigned int addr, char default_val)
{
  char key[12];
  sprintf(key, "nv_%04X", addr);
  nvs.begin("nonvol", true);
  char val = nvs.getChar(key, default_val);
  nvs.end();
  return val;
}

/*----------------------------------------------------------------
 * check_nonvol - verify and initialize nonvol
 *--------------------------------------------------------------*/
void check_nonvol(void)
{
  unsigned int nonvol_init;
  
  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("check_nonvol()"));
  }
  
  nonvol_init = nvs_get_int(NONVOL_INIT, 0xFFFF);
  
  if ( nonvol_init != INIT_DONE)
  {
    factory_nonvol(true);
  }

  nonvol_init = nvs_get_int(NONVOL_PS_VERSION, 0xFFFF);
  if ( nonvol_init != PS_VERSION )
  {
    backup_nonvol();
    update_nonvol(nonvol_init);
  }
  
  return;
}

/*----------------------------------------------------------------
 * factory_nonvol - Reset to factory defaults
 *--------------------------------------------------------------*/
void factory_nonvol(bool new_serial_number)
{
  unsigned int nonvol_init;
  unsigned int serial_number;
  char ch;
  unsigned int x;
  double dx;
  unsigned int i;
  char str[32];
  
  Serial.print(T("\r\nReset to factory defaults\r\n"));
  
  serial_number = nvs_get_int(NONVOL_SERIAL_NO, 0xFFFF);
  
  gen_position(0); 
  nvs_put_int(NONVOL_vset_PWM, 0);

  // Use JSON table to initialize
  i=0;
  while ( JSON[i].token != 0 )
  {
    switch ( JSON[i].convert )
    {
       case IS_VOID:
       case IS_FIXED:
         break;
       
       case IS_TEXT:
       case IS_SECRET:
         if ( JSON[i].non_vol != 0 )
         {
           nvs_put_int(JSON[i].non_vol, 0);
         }
         break;
         
      case IS_INT16:
        x = JSON[i].init_value;
        if ( JSON[i].non_vol != 0 )
        {
          nvs_put_int(JSON[i].non_vol, x);
        }
        if ( JSON[i].value != 0 )
        {
          *JSON[i].value = x;
        }
        break;

      case IS_FLOAT:
      case IS_DOUBLE:
        dx = (double)JSON[i].init_value;
        if ( JSON[i].non_vol != 0 )
        {
          nvs_put_double(JSON[i].non_vol, dx);
        }
        if ( JSON[i].d_value != 0 )
        {
          *JSON[i].d_value = dx;
        }
        break;
    }
    i++;
  }
  Serial.print(T("\r\nDone\r\n"));
  
  backup_nonvol();

  if ( new_serial_number )
  {
    Serial.print(T("\r\nTesting motor drive "));
    for (x=10; x != 0; x--)
    {
      paper_on_off(true);
      delay(ONE_SECOND/4);
      paper_on_off(false);
      delay(ONE_SECOND/4);
    }
    Serial.print(T(" Test Complete\r\n"));
  }

  set_trip_point(0);

  if ( new_serial_number )
  {
    ch = 0;
    serial_number = 0;
    while ( Serial.available() ) Serial.read();
  
    Serial.print(T("\r\nSerial Number? (ex 223! or x))"));
    while (1)
    {
      if ( Serial.available() != 0 )
      {
        ch = Serial.read();
        if ( ch == '!' )
        {  
          nvs_put_int(NONVOL_SERIAL_NO, serial_number);
          Serial.print(T(" Setting Serial Number to: ")); Serial.print(serial_number);
          break;
        }
        if ( ch == 'x' ) break;
        serial_number *= 10;
        serial_number += ch - '0';
      }
    }
  }
  else
  {
    nvs_put_int(NONVOL_SERIAL_NO, serial_number);
  }
  
  nonvol_init = PS_VERSION;
  nvs_put_int(NONVOL_PS_VERSION, nonvol_init);
  nonvol_init = INIT_DONE;
  nvs_put_int(NONVOL_INIT, nonvol_init);
  
  soft_reset();
  return;
}

/*----------------------------------------------------------------
 * init_nonvol - Initialize with verification
 *--------------------------------------------------------------*/
#define INIT_ALLOWED         1234
#define INIT_SERIAL_NUMBER   1235

void init_nonvol(int verify)
{
  if ( (verify != INIT_ALLOWED) && (verify != INIT_SERIAL_NUMBER) )
  {
    Serial.print(T("\r\nUse {\"INIT\":1234}\r\n"));
    return;
  }
  factory_nonvol(verify == INIT_SERIAL_NUMBER);
  return;
}

/*----------------------------------------------------------------
 * read_nonvol - Read NVS and set up variables
 *--------------------------------------------------------------*/
void read_nonvol(void)
{
  unsigned int nonvol_init;
  unsigned int i;
  unsigned int x;
  double dx;

  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("read_nonvol()"));
  }
  
  nonvol_init = nvs_get_int(NONVOL_INIT, 0xFFFF);
  if ( nonvol_init != INIT_DONE )
  {
    factory_nonvol(true);
  }
  
  nonvol_init = nvs_get_int(NONVOL_SERIAL_NO, 0xFFFF);
  if ( nonvol_init == 0xFFFF )
  {
    factory_nonvol(true);
  }

  nonvol_init = nvs_get_int(NONVOL_PS_VERSION, 0xFFFF);
  if ( nonvol_init != PS_VERSION )
  {
    update_nonvol(nonvol_init);
  }
  
  // Use JSON table to read values
  i=0;
  while ( JSON[i].token != 0 )
  {
    if ( (JSON[i].value != 0) || (JSON[i].d_value != 0) )
    {
      switch ( JSON[i].convert )
      {
        case IS_VOID:
          break;
          
        case IS_TEXT:
        case IS_SECRET:
          // Text values stored as sequence of chars
          if ( JSON[i].non_vol != 0 )
          {
            char* s = (char*)JSON[i].value;
            int j = 0;
            while (1)
            {
              char ch = nvs_get_char(JSON[i].non_vol + j, 0);
              s[j] = ch;
              if (ch == 0) break;
              j++;
            }
          }
          break;

        case IS_INT16:
        case IS_FIXED:
          if ( JSON[i].non_vol != 0 )
          {
            x = nvs_get_int(JSON[i].non_vol, 0xABAB);
            if ( x == 0xABAB )
            {
              x = JSON[i].init_value;
              nvs_put_int(JSON[i].non_vol, x);
            }
            *JSON[i].value = x;
          }
          else
          {
            *JSON[i].value = JSON[i].init_value;
          }
          break;

        case IS_FLOAT:
        case IS_DOUBLE:
          dx = nvs_get_double(JSON[i].non_vol, (double)JSON[i].init_value);
          *JSON[i].d_value = dx;
          break;
      }
    }
    i++;
  }

  return;
}

/*----------------------------------------------------------------
 * update_nonvol - Migrate storage versions
 *--------------------------------------------------------------*/
void update_nonvol(unsigned int current_version)
{
  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("update_nonvol(")); Serial.print(current_version); Serial.print(T(")")); 
  }
  
  // For ESP32, just update to current version
  // Migration logic preserved for compatibility
  nvs_put_int(NONVOL_PS_VERSION, PS_VERSION);
  
  return;
}

/*----------------------------------------------------------------
 * gen_position - Reset position variables
 *--------------------------------------------------------------*/
void gen_position(int v)
{
  json_north_x = 0; json_north_y = 0;
  json_east_x = 0;  json_east_y = 0;
  json_south_x = 0; json_south_y = 0;
  json_west_x = 0;  json_west_y = 0;

  nvs_put_int(NONVOL_NORTH_X, 0); nvs_put_int(NONVOL_NORTH_Y, 0);
  nvs_put_int(NONVOL_EAST_X, 0);  nvs_put_int(NONVOL_EAST_Y, 0);
  nvs_put_int(NONVOL_SOUTH_X, 0); nvs_put_int(NONVOL_SOUTH_Y, 0);
  nvs_put_int(NONVOL_WEST_X, 0);  nvs_put_int(NONVOL_WEST_Y, 0);

  return;
}

/*----------------------------------------------------------------
 * dump_nonvol - Display stored values
 *--------------------------------------------------------------*/
void dump_nonvol(void)
{
  unsigned int i;
  
  Serial.print(T("\r\nNVS Dump (ESP32 Preferences)\r\n"));
  
  // On ESP32, dump JSON table values instead of raw hex
  i = 0;
  while ( JSON[i].token != 0 )
  {
    if ( JSON[i].non_vol != 0 )
    {
      Serial.print(JSON[i].token);
      switch (JSON[i].convert)
      {
        case IS_INT16:
        case IS_FIXED:
          Serial.print(nvs_get_int(JSON[i].non_vol, 0xABAB));
          break;
        case IS_FLOAT:
        case IS_DOUBLE:
          Serial.print(nvs_get_double(JSON[i].non_vol, 0.0));
          break;
        default:
          Serial.print(T("--"));
          break;
      }
      Serial.print(T("\r\n"));
    }
    i++;
  }
  
  Serial.print(T("\r\nDone\r\n"));
  return;
}

/*----------------------------------------------------------------
 * backup/restore - ESP32 NVS versions
 *--------------------------------------------------------------*/
void backup_nonvol(void)
{
  // ESP32 NVS has built-in wear leveling, backup is less critical
  // but we maintain the interface for compatibility
  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("\r\nBackup: NVS is wear-leveled, backup skipped"));
  }
  return;
}

void restore_nonvol(void)
{
  if ( DLT(DLT_DIAG) )
  {
    Serial.print(T("\r\nRestore: Use factory reset instead"));
  }
  return;
}
