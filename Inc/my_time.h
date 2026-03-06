#pragma once
#include <stdint.h>
uint32_t millis32();
void set_alarm(uint16_t millisFromNow);
void wait_until_alarm_stopped();
void delay_stopped(uint16_t delay);

extern uint32_t rtc_ticks;