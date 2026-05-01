/*-------------------------------------------------------
 * 
 * token.ino
 * 
 * Token ring driver
 * VSKTarget - ESP32-S3 Port (Rev 6.0)
 * 
 * Changes: esp01_is_present() always returns true on ESP32
 * WiFi is built-in, so token ring check uses wifi_connected()
 * 
 * ------------------------------------------------------*/

#include "vskETarget.h"
#include "token.h"
#include "wifi.h"

int my_ring = TOKEN_UNDEF;
int whos_ring = TOKEN_UNDEF;

void token_init(void)
{
  if (json_token == TOKEN_WIFI)
  {
    return;    // WiFi mode, no token ring
  }

  if (DLT(DLT_CRITICAL))
  {
    Serial.print(T("token_init()"));
  }

  if (json_token == TOKEN_MASTER)
  {
    AUX_SERIAL.write((char)(TOKEN_BYTE | TOKEN_ENUM | 2));
  }
  else
  {
    AUX_SERIAL.write((char)(TOKEN_BYTE | TOKEN_ENUM_REQUEST | (my_ring & TOKEN_RING)));
  }

  return;
}

void token_poll(void)
{
  char token;

  switch (json_token)
  {
    case TOKEN_WIFI:
      while (aux_spool_available())
      {
        token = aux_spool_read();
        json_spool_put(token);
      }
      break;

    case TOKEN_MASTER:
      while (Serial.available())
      {
        token = Serial.read();
        AUX_SERIAL.write(token);
        json_spool_put(token);
      }

      while (aux_spool_available())
      {
        token = aux_spool_read();

        if (token & TOKEN_BYTE)
        {
          switch (token & TOKEN_CONTROL)
          {
            case TOKEN_ENUM_REQUEST:
              AUX_SERIAL.write((char)(TOKEN_BYTE | TOKEN_ENUM | 2));
              break;
            case TOKEN_ENUM:
              my_ring = 1;
              whos_ring = TOKEN_UNDEF;
              break;
            case TOKEN_TAKE:
              if ((token & TOKEN_RING) == my_ring) whos_ring = my_ring;
              break;
            case TOKEN_RELEASE:
              whos_ring = TOKEN_UNDEF;
              break;
            case TOKEN_TAKE_REQUEST:
              if (whos_ring == TOKEN_UNDEF)
              {
                whos_ring = token & TOKEN_RING;
                AUX_SERIAL.write((char)(TOKEN_BYTE | TOKEN_TAKE | (token & TOKEN_RING)));
              }
              break;
            case TOKEN_RELEASE_REQUEST:
              if (whos_ring == (token & TOKEN_RING))
              {
                whos_ring = TOKEN_UNDEF;
                AUX_SERIAL.write((char)(TOKEN_BYTE | TOKEN_RELEASE | (token & TOKEN_RING)));
              }
              break;
            default:
              Serial.write(token);
              break;
          }
        }
        else
        {
          if (whos_ring != TOKEN_UNDEF) Serial.write(token);
        }
      }
      break;

    case TOKEN_SLAVE:
      while (aux_spool_available())
      {
        token = aux_spool_read();

        if (token & TOKEN_BYTE)
        {
          switch (token & TOKEN_CONTROL)
          {
            case TOKEN_ENUM_REQUEST:
            case TOKEN_TAKE_REQUEST:
            case TOKEN_RELEASE_REQUEST:
              AUX_SERIAL.write(token);
              break;
            case TOKEN_ENUM:
              my_ring = token & TOKEN_RING;
              whos_ring = TOKEN_UNDEF;
              AUX_SERIAL.write((char)(TOKEN_BYTE | TOKEN_ENUM | (my_ring + 1)));
              break;
            case TOKEN_TAKE:
              whos_ring = token & TOKEN_RING;
              AUX_SERIAL.write(token);
              break;
            case TOKEN_RELEASE:
              whos_ring = TOKEN_UNDEF;
              AUX_SERIAL.write(token);
              break;
            default:
              AUX_SERIAL.write(token);
              break;
          }
        }
        else
        {
          AUX_SERIAL.write(token);
          if (whos_ring == TOKEN_UNDEF) json_spool_put(token);
        }
      }
      break;
  }

  return;
}

int token_take(void)
{
  if (json_token == TOKEN_WIFI) return 0;
  if (whos_ring != TOKEN_UNDEF) return 1;
  AUX_SERIAL.write((char)(TOKEN_BYTE | TOKEN_TAKE_REQUEST | my_ring));
  return 1;
}

int token_give(void)
{
  if (json_token == TOKEN_WIFI) return 0;
  if (whos_ring != my_ring) return 0;
  AUX_SERIAL.write((char)(TOKEN_BYTE | TOKEN_RELEASE_REQUEST | my_ring));
  return 1;
}

int token_available(void)
{
  if ((json_token == TOKEN_WIFI) ||
      ((whos_ring == my_ring) && (my_ring != TOKEN_UNDEF)))
  {
    return 1;
  }
  return 0;
}
