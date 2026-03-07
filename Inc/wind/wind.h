#pragma once
#include <stdint.h>

#define WIND_SAMPLE_SIZE 256

enum transducer_enum
{
    N = 0, 
    S = 1,
    E = 2,
    W = 3
} typedef transducer_t;

extern const uint8_t g_channel_transducers[6][2];
extern volatile uint16_t g_wind_measurement[WIND_SAMPLE_SIZE];
extern uint8_t g_windRingCounts[6];
extern uint32_t g_signalPowers[6][2];
