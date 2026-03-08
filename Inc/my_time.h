#pragma once
#include <stdint.h>
#include <stdbool.h>
uint32_t millis32();
void set_alarm(uint16_t millisFromNow);
void wait_until_alarm_stopped();
void delay_stopped(uint16_t delay);
void stop_until_event(bool returnToHSE);

extern uint32_t g_rtcTicks;
extern volatile bool g_canStop;