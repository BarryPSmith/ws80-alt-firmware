#pragma once

#include "stm32l1xx_hal.h"

void ProcessScope(uint8_t channel, uint8_t direction);
void InitScope();
void print_wind_phys_debug();
void adjustRingCounts();

#define SCOPE_BUFFER_SIZE 980
#define CAPTURE_COUNT 20
#define TRANSDUCER_FREQ 40000

// Have to be careful with this. Buffer must be aligned for DMA to work;
// But we want it to work over network - so packing would be ideal.
// The below works. But it is fragile. Not sure the best option,
// perhaps declare a larger array rather than a struct and manually fill
// in the inital bytes.
struct ScopePacket
{
    uint16_t Identifier1; // 2
    uint16_t Identifier2; // 2
    uint16_t PacketSize; // 2
    uint8_t ProgId; // 1
    uint8_t outputChannelId; // 1
    uint8_t inputChannelId; // 1
    uint8_t excitationCycleCount; // 1
    uint16_t temperature;
    uint16_t Buffer[SCOPE_BUFFER_SIZE]; // 1024
    uint16_t Captures[CAPTURE_COUNT];
};

extern struct ScopePacket scopePacket;

extern uint8_t scopeInChannel;