/*----------------------------------------------------------------
 *
 * token.h
 *
 * Header file for token ring functions
 * VSKTarget - ESP32-S3 Port (Rev 6.0)
 *
 * Unchanged from Arduino - token ring protocol is the same
 *
 *---------------------------------------------------------------*/
#ifndef _TOKEN_H_
#define _TOKEN_H_

void token_init(void);
int  token_take(void);
int  token_give(void);
int  token_available(void);
void token_poll(void);

extern int my_ring;
extern int whos_ring;

#define TOKEN_BYTE    0x80
#define TOKEN_ENUM_REQUEST    (0 << 3)
#define TOKEN_ENUM            (1 << 3)
#define TOKEN_TAKE_REQUEST    (2 << 3)
#define TOKEN_TAKE            (3 << 3)
#define TOKEN_RELEASE_REQUEST (4 << 3)
#define TOKEN_RELEASE         (5 << 3)
#define TOKEN_CONTROL ( TOKEN_ENUM_REQUEST | TOKEN_ENUM | TOKEN_TAKE_REQUEST | TOKEN_TAKE | TOKEN_RELEASE_REQUEST | TOKEN_RELEASE )

#define TOKEN_RING    0x07
#define TOKEN_WIFI    0x00
#define TOKEN_MASTER  0x01
#define TOKEN_SLAVE   0x02

#define TOKEN_UNDEF   -1
#define TOKEN_OWN     1

#define TOKEN_TIME_OUT  2000

#endif
