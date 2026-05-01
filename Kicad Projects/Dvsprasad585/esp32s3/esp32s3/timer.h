/*----------------------------------------------------------------
 *
 * timer.h
 *
 * Header file for timer functions
 * VSKTarget - ESP32-S3 Port (Rev 6.1)
 *
 *---------------------------------------------------------------*/
#ifndef _TIMER_H_
#define _TIMER_H_

/*
 * Function Prototypes
 */
void init_timer(void);
void enable_timer_interrupt(void);
void disable_timer_interrupt(void);
unsigned int timer_new(unsigned long* new_timer, unsigned long start);
unsigned int timer_delete(unsigned long* old_timer);

/*
 * Timers
 */
extern volatile unsigned long  keep_alive;
extern volatile unsigned long  tabata_rapid_timer;
extern volatile unsigned long  in_shot_timer;
extern volatile unsigned long  power_save;
extern volatile unsigned long  token_tick;

#endif
