#include "radio.h"
#include "wind.h"
#include "temperature.h"
#include "my_time.h"
#include <stdbool.h>
#include <string.h>

const uint8_t radio_tx_interval = (uint8_t)(RADIO_TX_PERIOD * WAKEUP_FREQUENCY);

uint32_t last_tx;

struct radioPacket
{
    uint8_t b1;
    uint8_t id[3];
    uint8_t light_hi;
    uint8_t light_lo;
    uint8_t battery;
    uint8_t hiBits;
    uint8_t temp_lo;
    uint8_t humidity;
    uint8_t wind_avg_lo;
    uint8_t wind_dir_lo;
    uint8_t wind_max_lo;
    uint8_t uv_index_x10;
    uint8_t b14;
    uint8_t b15;
    uint8_t crc1;
    uint8_t chkSum;
} typedef RadioPacketTypedef;

//TODO: All of these
uint8_t radio_id[3];
int16_t light; 
uint8_t battery;
uint8_t pressure;
uint8_t uvIndex_x10;
void CreateRadioPacket(RadioPacketTypedef *packet)
{
    uint16_t avg_dmps, gust_dmps, angle_deg;
    get_wind_parameters(&avg_dmps, &gust_dmps, &angle_deg);
    int16_t tempPlus400 = lastTempMeasurement + 400;

    packet->b1 = 0x80;
    memcpy(radio_id, packet->id, sizeof(packet->id));
    packet->light_hi = ((uint16_t)light) >> 8;
    packet->light_lo = light & 0xFF;
    packet->battery = battery;
    packet->hiBits =
        // Fuck the minVoltages. They're in bit 7 and bit 3.
        ((gust_dmps & 0x100) >> 2) |
        ((angle_deg & 0x100) >> 3) |
        ((avg_dmps & 0x100) >> 4) |
        ((tempPlus400 & 0x700) >> 8);
    packet->temp_lo = tempPlus400 & 0xFF;
    packet->humidity = lastHumidityMeasurement & 0xFF;
    packet->wind_avg_lo = avg_dmps & 0xFF;
    packet->wind_dir_lo = angle_deg & 0xFF;
    packet->wind_max_lo = gust_dmps & 0xFF;
    packet->uv_index_x10 = uvIndex_x10;
    // TODO: The other rubbish, like checksum.
}

bool processRadio()
{
    uint32_t localTicks = g_rtcTicks;
    if (localTicks - last_tx > radio_tx_interval)
    {
        last_tx = g_rtcTicks;
        RadioPacketTypedef radioPacket;
        CreateRadioPacket(&radioPacket);
        radio_transmit(&radioPacket, sizeof(radioPacket));
        return true;
    }
    return false;
}