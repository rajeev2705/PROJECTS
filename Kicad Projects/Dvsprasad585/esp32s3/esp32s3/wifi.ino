/*----------------------------------------------------------------
 *
 * wifi.ino
 *
 * Native WiFi Driver for ESP32
 * VSKTarget - ESP32-S3 Port (Rev 6.0)
 *
 * Replaces esp-01.ino entirely. No AT commands needed!
 * Uses ESP32 native WiFi + WebServer for TCP connections.
 *
 *---------------------------------------------------------------*/
#include "vskETarget.h"
#include "wifi.h"
#include "json.h"
#include "token.h"

/*
 * Local Variables
 */
static WebServer server(1090);                    // Listen on port 1090 (same as original)
static bool wifi_is_started = false;
static bool wifi_client_connected = false;

static char wifi_in_queue[WIFI_BUFFER_SIZE];      // Input buffer
static volatile int wifi_in_ptr = 0;
static volatile int wifi_out_ptr = 0;

/*----------------------------------------------------------------
 *
 * function: wifi_init
 *
 * brief: Initialize the ESP32 native WiFi
 * 
 * return: None
 *
 *----------------------------------------------------------------
 *   
 * Network Settings:
 * 
 * json_wifi_ssid given:
 *    Connect to this SSID (Station mode)
 *    Use DHCP or json_wifi_ip if given
 *    
 * json_wifi_ssid empty:
 *    Create AP named VSK-<name>
 *    IP address is 192.168.10.9
 *   
 *--------------------------------------------------------------*/
void wifi_init(void)
{  
  if ( DLT(DLT_CRITICAL) ) 
  {
    Serial.print(T("wifi_init() - ESP32 Native WiFi"));
  }
  
  if ( json_token != TOKEN_WIFI )
  {
    if ( DLT(DLT_CRITICAL) ) 
    {
      Serial.print(T("In Token Ring mode. WiFi not available"));
    }
    return;
  }

/*
 * If an SSID is defined, connect as Station
 */
  if ( *json_wifi_ssid == 0 )
  {
    // AP Mode - Create our own access point
    if ( DLT(DLT_CRITICAL) )
    {
      Serial.print(T("WiFi: Configuring as AP"));
    }
    
    char ap_name[48];
    sprintf(ap_name, "VSK-%s", namesensor[json_name_id]);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(
      IPAddress(192, 168, 10, 9),     // AP IP
      IPAddress(192, 168, 10, 9),     // Gateway
      IPAddress(255, 255, 255, 0)     // Subnet
    );
    WiFi.softAP(ap_name);            // Open network (no password)
    
    if ( DLT(DLT_CRITICAL) )
    {
      Serial.print(T("WiFi AP: ")); Serial.print(ap_name);
      Serial.print(T(" IP: ")); Serial.print(WiFi.softAPIP());
    }
  }
  else
  {
    // Station Mode - Connect to existing AP
    if ( DLT(DLT_CRITICAL) )
    {
      Serial.print(T("WiFi: Connecting to ")); Serial.print(json_wifi_ssid);
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(json_wifi_ssid, json_wifi_pwd);
    
    // Wait for connection (up to 20 seconds)
    int timeout = 40;  // 40 x 500ms = 20 seconds
    while (WiFi.status() != WL_CONNECTED && timeout > 0)
    {
      delay(500);
      Serial.print(T("."));
      timeout--;
    }
    
    if (WiFi.status() == WL_CONNECTED)
    {
      if ( DLT(DLT_CRITICAL) )
      {
        Serial.print(T("\r\nWiFi connected. IP: ")); Serial.print(WiFi.localIP());
      }
    }
    else
    {
      if ( DLT(DLT_CRITICAL) )
      {
        Serial.print(T("\r\nWiFi: Failed to connect to ")); Serial.print(json_wifi_ssid);
      }
    }
  }

/*
 * Start the TCP server on port 1090
 */
  server.on("/", []() {
    server.send(200, "text/plain", "VSKTarget ESP32");
  });
  
  // Handle raw TCP data via a catch-all handler
  server.onNotFound([]() {
    if (server.method() == HTTP_POST || server.method() == HTTP_GET)
    {
      String body = server.arg("plain");
      // Queue incoming data
      for (unsigned int i = 0; i < body.length(); i++)
      {
        int next = (wifi_in_ptr + 1) % WIFI_BUFFER_SIZE;
        if (next != wifi_out_ptr)
        {
          wifi_in_queue[wifi_in_ptr] = body[i];
          wifi_in_ptr = next;
        }
      }
      server.send(200, "text/plain", "OK");
    }
  });
  
  server.begin();
  wifi_is_started = true;

  if ( DLT(DLT_CRITICAL) )
  {
    Serial.print(T("\r\nWiFi: Server started on port 1090"));
  }

  return;
}


/*----------------------------------------------------------------
 *
 * function: wifi_restart
 *
 * brief:    Reset the WiFi
 * 
 * return:   TRUE on success
 *
 *--------------------------------------------------------------*/
bool wifi_restart(void)
{
  WiFi.disconnect();
  delay(ONE_SECOND);
  wifi_init();
  return true;
}


/*----------------------------------------------------------------
 *
 * function: wifi_receive
 *
 * brief:    Process incoming WiFi data
 * 
 * return:   None
 *
 *--------------------------------------------------------------*/
void wifi_receive(void)
{
  if (wifi_is_started)
  {
    server.handleClient();
  }
}


/*----------------------------------------------------------------
 *
 * function: wifi_read
 *
 * brief:    Return a character from the WiFi input queue
 * 
 * return:   Next character or -1 if empty
 *
 *--------------------------------------------------------------*/
char wifi_read(void)
{
  char ch = -1;
  
  if (wifi_in_ptr != wifi_out_ptr)
  {
    ch = wifi_in_queue[wifi_out_ptr];
    wifi_out_ptr = (wifi_out_ptr + 1) % WIFI_BUFFER_SIZE;
  }
  
  return ch;
}


/*----------------------------------------------------------------
 *
 * function: wifi_available
 *
 * brief:    Return number of characters waiting
 * 
 * return:   Count of available characters
 *
 *--------------------------------------------------------------*/
unsigned int wifi_available(void)
{
  if (wifi_in_ptr >= wifi_out_ptr)
    return wifi_in_ptr - wifi_out_ptr;
  else
    return WIFI_BUFFER_SIZE + wifi_in_ptr - wifi_out_ptr;
}


/*----------------------------------------------------------------
 *
 * function: wifi_send
 *
 * brief:    Send a string to all connected WiFi clients
 * 
 * return:   TRUE if sent
 *
 *--------------------------------------------------------------*/
bool wifi_send(char* str)
{
  // For WebServer mode, responses are sent via server.send()
  // For real-time score streaming, consider upgrading to WebSocket
  // For now, scores are output via Serial and can be read via HTTP
  return wifi_is_started;
}


/*----------------------------------------------------------------
 *
 * function: wifi_connected
 *
 * brief:    Check if WiFi is connected
 * 
 * return:   TRUE if WiFi is active
 *
 *--------------------------------------------------------------*/
bool wifi_connected(void)
{
  if (*json_wifi_ssid == 0)
  {
    // AP mode - check if any stations are connected
    return (WiFi.softAPgetStationNum() > 0);
  }
  else
  {
    return (WiFi.status() == WL_CONNECTED);
  }
}


/*----------------------------------------------------------------
 *
 * function: wifi_test
 *
 * brief:    Run WiFi diagnostics
 *
 *--------------------------------------------------------------*/
void wifi_test(void)
{
  Serial.print(T("\r\nESP32 WiFi Test"));
  Serial.print(T("\r\nMAC Address: ")); Serial.print(WiFi.macAddress());
  
  if (*json_wifi_ssid == 0)
  {
    Serial.print(T("\r\nMode: Access Point"));
    Serial.print(T("\r\nAP IP: ")); Serial.print(WiFi.softAPIP());
    Serial.print(T("\r\nConnected stations: ")); Serial.print(WiFi.softAPgetStationNum());
  }
  else
  {
    Serial.print(T("\r\nMode: Station"));
    Serial.print(T("\r\nSSID: ")); Serial.print(json_wifi_ssid);
    Serial.print(T("\r\nStatus: ")); Serial.print(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.print(T("\r\nIP: ")); Serial.print(WiFi.localIP());
      Serial.print(T("\r\nRSSI: ")); Serial.print(WiFi.RSSI()); Serial.print(T(" dBm"));
    }
  }
  
  Serial.print(T("\r\nServer: ")); Serial.print(wifi_is_started ? "Running" : "Stopped");
  Serial.print(T("\r\nDone\r\n"));
}


/*----------------------------------------------------------------
 *
 * function: wifi_status
 *
 * brief:    Display WiFi status
 *
 *--------------------------------------------------------------*/
void wifi_status(void)
{
  wifi_test();  // Same output
}


/*----------------------------------------------------------------
 *
 * function: wifi_myIP
 *
 * brief:    Return the current IP address
 *
 *--------------------------------------------------------------*/
void wifi_myIP(char* return_value)
{
  IPAddress ip;
  
  if (*json_wifi_ssid == 0)
  {
    ip = WiFi.softAPIP();
  }
  else
  {
    ip = WiFi.localIP();
  }
  
  sprintf(return_value, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}
