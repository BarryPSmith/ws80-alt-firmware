
#include "my_time.h"
#include "wind.h"
#include "wind_phys.h"
#include "wind_calc.h"

volatile uint16_t g_wind_measurement[WIND_SAMPLE_SIZE];
uint8_t g_windRingCounts[6];

static uint32_t s_last_wind_sample;

const uint32_t wind_sample_rate = 1;
const uint8_t max_volume = 12;

void process_wind()
{
    if (g_rtcTicks - s_last_wind_sample < wind_sample_rate)
    {
        sample_wind();
    }
}

uint8_t g_channel_transducers[6][2] =
{ { 0, 1 },
  { 0, 2 },
  { 0, 3 },
  { 1, 2 },
  { 1, 3 },
  { 2, 3 } };

// We need six volume values
// 01  02  03  12  13  23
// NS, NE, NW, SE, SW, EW
uint8_t g_windRingCounts[6];
uint32_t g_signalPowers[6][2];

void sample_Wind()
{
    for (uint8_t dir = 0; dir < 2; dir++)
    {
        for (uint8_t chan = 0; chan < 6; chan++)
        {
            prepareToMeasureWind(chan, dir);
            measureWind();
            doneMeasureWind();
            processWindWaveform(chan, dir);
            delay_stopped(8);
        }
    }
    updatePhysicalParamters();
    int16_t x_cmps, y_cmps;
    calculate_wind(&x_cmps, &y_cmps);
    store_wind_sample(x_cmps, y_cmps);
}


