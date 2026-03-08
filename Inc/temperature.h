#pragma once
#include "stm32l1xx.h"

void InitTemperature();
void ProcessTemperature();

extern uint16_t lastTempMeasurement;
extern uint16_t lastHumidityMeasurement;
