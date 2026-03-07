#include "wind.h"

void processWindWaveform(uint8_t channel, uint8_t direction);
void calculate_wind(int16_t *x_cmps, int16_t *y_cmps);
void store_wind_sample(int16_t x_cmps, int16_t y_cmps);