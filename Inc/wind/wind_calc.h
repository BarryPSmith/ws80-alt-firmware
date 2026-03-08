#include "wind.h"

void processWindWaveform(uint8_t channel, uint8_t direction);
void calculate_wind(int16_t *x_cmps, int16_t *y_cmps);
void store_wind_sample(int16_t x_cmps, int16_t y_cmps);
void get_wind_parameters(uint16_t* pAvg_dmps, uint16_t* pGust_dmps, uint16_t* pAngle_deg);