#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include "usbd_cdc_if.h"

void debug_println(char* format, ...)
{
    va_list args;
    va_start(args, format);
    char buffer[256];
    uint8_t len = vsnprintf(buffer, sizeof(buffer), format, args);
    CDC_Transmit_FS((uint8_t*)buffer, len);
    HAL_Delay(1);
}