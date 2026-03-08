#include "wind.h"
#include "stm32l1xx_hal.h"
#include <stdbool.h>

static GPIO_TypeDef* s_transducerGPIOs[] = { GPIOB, GPIOB,GPIOA, GPIOB };
static uint8_t s_transducerPins[4][2] = {
    { 10, 11 },
    { 14, 15 },
    { 1,  8 },
    { 8,  9 } 
};

static volatile bool s_dma_complete;
static uint8_t s_ringCount;
static volatile uint8_t s_tim9Ticks;
static uint16_t s_togglePins, s_togglePin0;
static volatile uint32_t* s_toggleODR;
static GPIO_TypeDef* s_toggleGpio;

uint8_t LoadRingCount(transducer_t transmitter, transducer_t receiver);
void setupOutChannel(transducer_t transmitter);
void setupInChannel(transducer_t receiver);

void prepareToMeasureWind(uint8_t channel, uint8_t direction)
{
    transducer_t transmitter, receiver;
    
    transmitter = g_channel_transducers[channel][direction];
    receiver = g_channel_transducers[channel][(direction + 1) % 2];
    // Turn on the analog power
    GPIOA->BSRR = 1 << (4 + 16);

    // Get the timers ready
    s_tim9Ticks = 0;
    TIM9->CNT = 0;
    TIM2->CNT = 0;
    TIM2->EGR |= TIM_EGR_UG;
    TIM9->EGR |= TIM_EGR_UG;

    // Prepare ADC
    // Reset "Use DMA"
    ADC1->CR2 &= ~ADC_CR2_DMA;
    ADC1->CR2 |= ADC_CR2_DMA;
    // Clear ADC_SR
    ADC1->SR &= ~0x1Ful;
    ADC1->CR2 |= ADC_CR2_ADON;

    // Prepare DMA
    DMA1->IFCR |= 0x0FFFFFFF;
    DMA1_Channel1->CMAR = (uint32_t)&g_wind_measurement; // Apparently we don't need to set this; but I'm paranoid.
    DMA1_Channel1->CNDTR = (DMA1_Channel1->CNDTR & 0xFFFF0000) | WIND_SAMPLE_SIZE; 
    DMA1_Channel1->CCR |= DMA_CCR_EN;

    // Load the ring count
    s_ringCount = g_windRingCounts[channel];
    setupOutChannel(transmitter);
    setupInChannel(receiver);

    // Wait for the ADC
    while ((ADC1->SR & ADC_SR_ADONS) == 0);
}

void measureWind()
{
    // Wait to ensure the internal reference voltage is ready
    // (We might turn it off during some sleeps).
    while ((PWR->CSR & PWR_CSR_VREFINTRDYF) == 0)
    {}
    s_dma_complete = false;
    // Enabling timer 9 also enables timer 2.
    // Timer 9 will ring the transmitter using its interrupt
    // When timer 2 reaches a full count (~6000), it will start the ADC
    // The ADC will run with DMA, until it's filled the buffer
    // Then the DMA interrupt will fire, and s_dma_complete will be set.
    TIM9->CR1 |= TIM_CR1_CEN;
    while (!s_dma_complete)
    {
        // Nothing to do while our interrupts, timers, and DMA handle the measurement.
        // We stay in full power for the ADC, &c, but shut down the core to reduce consumption.
        __WFE();
    }
}

void doneMeasureWind()
{
    // Turn off the ADC
    ADC1->CR2 &= ~ADC_CR2_ADON;
    // Turn off the analog circuitry:
    GPIOA->BSRR = 1 << (4);
    // Turn off the DMA
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
}

void setupOutChannel(transducer_t transmitter)
{
    s_togglePins = (1 << s_transducerPins[transmitter][0]) | (1 << s_transducerPins[transmitter][1]);
    s_toggleGpio = s_transducerGPIOs[transmitter];
    s_toggleODR = &s_toggleGpio->ODR;
    s_togglePin0 = 1 << s_transducerPins[transmitter][0];
}

void setupInChannel(transducer_t receiver)
{
    // Set the ADC Mux:
    uint8_t p6Val = (receiver & 1) << 6;
    GPIOA->ODR = (GPIOA->ODR & ~GPIO_ODR_ODR_6) | p6Val;
    uint8_t p7Val = (receiver & 2) << 6;
    GPIOA->ODR = (GPIOA->ODR & ~GPIO_ODR_ODR_7) | p7Val;

    for (int i = 0; i < 4; i++)
    {
        GPIO_TypeDef* gpio = s_transducerGPIOs[i];
        uint8_t offset = s_transducerPins[i][1];
        if (i != receiver)
        {
            // Set the pin to output
            gpio->MODER = (gpio->MODER & ~(3 << (offset * 2))) | MODE_OUTPUT << (offset * 2);
            // Set it to high speed:
            gpio->OSPEEDR |= GPIO_SPEED_FREQ_VERY_HIGH << (offset * 2);
            // Set it LOW
            gpio->BSRR = 1 << (offset + 16);
        }
        else
        {
            // Set the pin to input
            gpio->MODER &= ~(3 << (offset * 2));
        }
    }
}

void updatePhysicalParamters()
{
    const uint32_t min_power = 25000;
    const uint32_t barrier_power = 45000;
    const uint32_t max_power = 90000;
    const uint8_t minRings = 2;
    const uint8_t maxRings = 12;
    for (int channel = 0; channel < 6; channel++)
    {
        if (
                (g_signalPowers[channel][0] > max_power ||
                    g_signalPowers[channel][1] > max_power) &&
                g_windRingCounts[channel] > minRings)
            g_windRingCounts[channel]--;
        else if (
                (g_signalPowers[channel][0] < min_power ||
                    g_signalPowers[channel][1] < min_power) &&
                (g_signalPowers[channel][0] < barrier_power &&
                    g_signalPowers[channel][1] < barrier_power) &&
                g_windRingCounts[channel] < maxRings)
            g_windRingCounts[channel]++;
    }
}

void timer9_tick()
{
    if (s_tim9Ticks == 0)
    *s_toggleODR |= s_togglePin0;
    else
        *s_toggleODR ^= s_togglePins;

    if (s_tim9Ticks > s_ringCount)
        return;

    if (s_tim9Ticks == s_ringCount)
    {
        // Turn off both toggle pins
        *s_toggleODR &= ~s_togglePins;
        // Turn off our timer
        TIM9->CR1 &= ~TIM_CR1_CEN;
    }

    s_tim9Ticks++;
}


void dma1_channel1_irq()
{
    s_dma_complete = true;
}