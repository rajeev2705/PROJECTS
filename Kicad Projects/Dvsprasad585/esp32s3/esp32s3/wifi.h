/*----------------------------------------------------------------
 *
 * wifi.h
 *
 * Native WiFi Driver for ESP32
 * VSKTarget - ESP32-S3 Port (Rev 6.0)
 *
 * Replaces esp-01.h - no external WiFi module needed!
 * ESP32 has built-in WiFi and acts as both AP and Station.
 *
 *---------------------------------------------------------------*/

#ifndef _WIFI_H_
#define _WIFI_H_

/*
 * Function Prototypes
 */
void         wifi_init(void);                     // Initialize WiFi
bool         wifi_restart(void);                  // Reset WiFi

char         wifi_read(void);                     // Read a character from the queue
unsigned int wifi_available(void);                // Return the number of available characters
bool         wifi_send(char* str);                // Send out a string to all connected clients
void         wifi_receive(void);                  // Process incoming WiFi data
bool         wifi_connected(void);                // TRUE if any client is connected
void         wifi_test(void);                     // Diagnostic self test
void         wifi_status(void);                   // Display the WiFi status
void         wifi_myIP(char* s);                  // Obtain the working IP address

/*
 * Definitions
 */
#define WIFI_MAX_CLIENTS    3                     // Allow up to 3 WebSocket connections
#define WIFI_BUFFER_SIZE    2048                   // Input buffer size

#define WIFI_SSID_SIZE      (32+1)                 // SSID storage
#define WIFI_PWD_SIZE       17                     // Password storage
#define WIFI_IP_SIZE        17                     // IP address storage

/*
 * Backwards compatibility aliases for code that references esp01_*
 */
#define esp01_init()           wifi_init()
#define esp01_restart()        wifi_restart()
#define esp01_read()           wifi_read()
#define esp01_available()      wifi_available()
#define esp01_send(str, ch)    wifi_send(str)
#define esp01_receive()        wifi_receive()
#define esp01_connected()      wifi_connected()
#define esp01_is_present()     true                // WiFi is always present on ESP32
#define esp01_test()           wifi_test()
#define esp01_status()         wifi_status()
#define esp01_myIP(s)          wifi_myIP(s)
#define esp01_close(ch)        ((void)0)

/* Size aliases for nonvol compatibility */
#define esp01_SSID_SIZE        WIFI_SSID_SIZE
#define esp01_SSID_SIZE_32     WIFI_SSID_SIZE
#define esp01_PWD_SIZE         WIFI_PWD_SIZE
#define esp01_IP_SIZE          WIFI_IP_SIZE
#define esp01_N_CONNECT        WIFI_MAX_CLIENTS

#endif
