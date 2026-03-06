#include "my_time.h"
#include "stm32l1xx_hal.h"
#include "stm32l1xx_hal_rtc.h"
#include <time.h>
#include <stdbool.h>
#include "stm32l1xx_hal_pwr.h"

uint32_t rtc_ticks = 0;

extern RTC_HandleTypeDef hrtc;

// Technically not millis, since it's 1024 ticks per second not 1000.
uint32_t millis32()
{
    RTC_TimeTypeDef rtcTime;
    RTC_DateTypeDef rtcDate;
    HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN);
    uint8_t hh = rtcTime.Hours;
    uint8_t mm = rtcTime.Minutes;
    uint8_t ss = rtcTime.Seconds;
    uint8_t d = rtcDate.Date;
    uint8_t m = rtcDate.Month;
    uint16_t y = rtcDate.Year;
    uint16_t yr = (uint16_t)(y+2000-1900);
    uint32_t currentTime;
    struct tm tim = {0};
    tim.tm_year = yr;
    tim.tm_mon = m - 1;
    tim.tm_mday = d;
    tim.tm_hour = hh;
    tim.tm_min = mm;
    tim.tm_sec = ss;
    currentTime = mktime(&tim);
    return currentTime * 1024 + rtcTime.SubSeconds;
}

uint32_t alarmMillis;
volatile bool _alarmSignalled;
void set_alarm(uint16_t millisFromNow)
{
    uint32_t curMillis = millis32();
    alarmMillis = curMillis + millisFromNow;
    uint32_t alarmSeconds = alarmMillis / 1024;
    //struct tm* tim = localtime(&alarmSeconds);
    RTC_AlarmTypeDef rtcAlarm = {0};
    /*RTC_TimeTypeDef rtcTime;
    
    RTC_DateTypeDef rtcDate;
    rtcDate.Year = tim->tm_year;
    rtcDate.Month = tim->tm_mon;
    rtcDate.Date = tim->tm_mday;
    rtcAlarm.AlarmTime.Hours = tim->tm_hour;
    rtcAlarm.AlarmTime.Minutes = tim->tm_min;
    */
    rtcAlarm.AlarmTime.Seconds = alarmSeconds % 60;
    rtcAlarm.AlarmTime.SubSeconds = alarmMillis % 1024;
    rtcAlarm.AlarmMask = RTC_ALARMMASK_SECONDS;
    rtcAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
    rtcAlarm.Alarm = RTC_CR_ALRAE;
    _alarmSignalled = false;
    HAL_RTC_SetAlarm_IT(&hrtc, &rtcAlarm, RTC_FORMAT_BIN);
}

void wait_until_alarm_stopped()
{
    uint32_t old_rcc_cfgr = RCC->CFGR;
    while (!_alarmSignalled)
    {
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    }
    // STOP mode changed us to MSI instead of HSE. change it back:
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));
    RCC->CFGR = old_rcc_cfgr;

    HAL_RTC_DeactivateAlarm(&hrtc, RTC_CR_ALRAE);
}

void delay_stopped(uint16_t delay)
{
    set_alarm(delay);
    wait_until_alarm_stopped();
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
    _alarmSignalled = true;
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    rtc_ticks++;
}