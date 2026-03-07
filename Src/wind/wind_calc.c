#include "wind_calc.h"
#include <math.h>

#define SAMPLE_BUFFER_SIZE 60

// These are cos and sin with range -2048 -> +2048 == -2^11 -> + 2^11
// Period is 24.5 samples.
static int16_t sin24_5[49] = {0,519,1004,1424,1751,1963,2046,1996,1815,1516,1117,645,131,-391,-888,-1327,-1679,-1921,-2038,-2021,-1872,-1601,-1224,-768,-261,261,768,1224,1601,1872,2021,2038,1921,1679,1327,888,391,-131,-645,-1117,-1516,-1815,-1996,-2046,-1963,-1751,-1424,-1004,-519};
static int16_t cos24_5[49] = {2048,1981,1784,1471,1061,582,65,-455,-947,-1376,-1716,-1943,-2043,-2010,-1845,-1559,-1171,-707,-196,326,828,1276,1641,1898,2031,2031,1898,1641,1276,828,326,-196,-707,-1171,-1559,-1845,-2010,-2043,-1943,-1716,-1376,-947,-455,65,582,1061,1471,1784,1981};
// Window is a hann window, with maximum value range 0 -> 4096 == 0 -> 2^12
// double Hann(int i) => 2048 * Math.Sin(Math.PI * (i + 2) / (DataSize + 3)) * Math.Sin(Math.PI * (i + 2) / (DataSize + 3));
static int16_t window[245] = {2,5,10,16,23,32,41,53,65,79,93,110,127,146,165,187,209,232,257,283,309,337,367,397,428,460,494,528,563,599,636,675,713,753,794,835,878,921,964,1009,1054,1099,1146,1192,1240,1288,1336,1385,1434,1484,1534,1585,1635,1686,1737,1789,1840,1892,1944,1996,2048,2099,2151,2203,2255,2306,2358,2409,2460,2510,2561,2611,2661,2710,2759,2807,2855,2903,2949,2996,3041,3086,3131,3174,3217,3260,3301,3342,3382,3420,3459,3496,3532,3567,3601,3635,3667,3698,3728,3758,3786,3812,3838,3863,3886,3908,3930,3949,3968,3985,4002,4016,4030,4042,4054,4063,4072,4079,4085,4090,4093,4095,4096,4095,4093,4090,4085,4079,4072,4063,4054,4042,4030,4016,4002,3985,3968,3949,3930,3908,3886,3863,3838,3812,3786,3758,3728,3698,3667,3635,3601,3567,3532,3496,3459,3420,3382,3342,3301,3260,3217,3174,3131,3086,3041,2996,2949,2903,2855,2807,2759,2710,2661,2611,2561,2510,2460,2409,2358,2306,2255,2203,2151,2099,2048,1996,1944,1892,1840,1789,1737,1686,1635,1585,1534,1484,1434,1385,1336,1288,1240,1192,1146,1099,1054,1009,964,921,878,835,794,753,713,675,636,599,563,528,494,460,428,397,367,337,309,283,257,232,209,187,165,146,127,110,93,79,65,53,41,32,23,16,10,5,2};

static int16_t s_samples[SAMPLE_BUFFER_SIZE][2];
static uint8_t s_sample_write_idx;

typedef int16_t q2_13, q1_14;
typedef int32_t q18_13, q17_14;
#define qn_13_pi 25736
q2_13 s_signalPhases[6][2];

#define qn_14_sqrt1_2 11585 // sqrt(1/2)
#define qn_14_one 16384
q1_14 s_radiusVectors[6][2] = 
{ { qn_14_sqrt1_2 , -qn_14_sqrt1_2 } , // 0, 1
  { -qn_14_sqrt1_2, -qn_14_sqrt1_2 } , // 0, 2
  { 0,        -qn_14_one     } , // 0, 3
  { -qn_14_one,     0        } , // 1, 2
  { -qn_14_sqrt1_2, -qn_14_sqrt1_2 } , // 1, 3
  { qn_14_sqrt1_2 , -qn_14_sqrt1_2 } };// 2, 3
q1_14 s_tangentVectors[6][2] = 
{ {  qn_14_sqrt1_2,  qn_14_sqrt1_2 } ,
  {  qn_14_sqrt1_2, -qn_14_sqrt1_2 } ,
  {  qn_14_one    ,  0       } ,
  {  0      ,  qn_14_one     } ,
  {  qn_14_sqrt1_2, -qn_14_sqrt1_2 } ,
  {  qn_14_sqrt1_2,  qn_14_sqrt1_2 } };

void processWindWaveform(uint8_t channel, uint8_t direction)
{
    // Our source data is 12 bit ADC values. If we don't normalise them,
    // they have a maximum value of 2^12 each
    // It probably makes the most sense to have window and the trig functions 
    // have the same resolution as our data
    // Our data will range from -2^12 to 2^12
    // So cos will range from -2^11 to +2^11
    // window will range from 0 to 2^12
    // Multiply all three, and we get 35 bits.
    // So we need to lose 4 bits before the final multiplication.
    // Then, sum 2^8 values
    // So we need to lose 12 bits in total.

    const uint8_t sampleMax = 245; // 49 * 5;
    const uint8_t cycleLength = 49;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < sampleMax; i++)
        sum += g_wind_measurement[i];
    uint32_t avg = sum / sampleMax;
    uint32_t cosSum = 0;
    uint32_t sinSum = 0;
    for (uint8_t i = 0; i < sampleMax; i++)
    {
        int16_t rawVal = g_wind_measurement[i];
        int32_t val = (rawVal - avg) * window[i] >> 4; // rawVal max value is 2^12, window maxvalue is 2^12, Maximum intermediate value is 2^24, val maxvalue is 2^20
        int32_t cosVal = val * cos24_5[i % cycleLength] >> 8; //2^20, 2^11, maximum intermediate 2^31, cosVal maxvalue is 2^23
        cosSum += cosVal; // 2^23, 2^8 times, maximum value is 2^31
        int32_t sinVal = val * sin24_5[i % cycleLength] >> 8; // as above
        sinSum += sinVal; // as above
    }
    const uint32_t window_sum = 507782; // 2^19
    uint16_t cos16 = cosSum / window_sum; // maximum of 2^12. More likely 2^8 or less.
    uint16_t sin16 = sinSum / window_sum;
    uint32_t power = cos16 * cos16 + sin16 * sin16; // Maximum of 2^24
    g_signalPowers[channel][direction] = power;

    double phase = atan2(sinSum, cosSum);
    s_signalPhases[channel][direction] = phase * (1 << 13); // phase maximum +/- pi, this has maximum value +/- 2^15
}

q18_13 normalise_angle(q18_13 angle)
{
    while (angle < -qn_13_pi)
        angle += 2 * qn_13_pi;
    while (angle > qn_13_pi)
        angle -= qn_13_pi;
}

void calculate_wind(int16_t *x_cmps, int16_t *y_cmps)
{
    // time in nanoseconds
    q18_13 signalPhaseDiffs[6];
    for (uint8_t channel = 0; channel < 6; channel++)
    {
        q18_13 phaseDiff = normalise_angle(
            (q18_13)s_signalPhases[channel][0] - s_signalPhases[channel][1]);
        signalPhaseDiffs[channel] = phaseDiff; // maximum 2^15
    }
    uint32_t best_residual = 0xFFFFFFFF;
    uint16_t best_x;
    uint16_t best_y;
    for (int8_t ch2Wrap = -1; ch2Wrap <= 1; ch2Wrap++)
    {
        for (int8_t ch3Wrap = -1; ch3Wrap <= 1; ch3Wrap++)
        {
            int32_t sum_x = 0, 
                    sum_y = 0;
            for (uint8_t channel = 0; channel < 6; channel++)
            {
                q18_13 phaseDiff = signalPhaseDiffs[channel];
                if (channel == 2)
                    phaseDiff += 2 * qn_13_pi * ch2Wrap;
                if (channel == 3)
                    phaseDiff += 2 * qn_13_pi * ch3Wrap;
                int16_t x, y;
                get_radius_vector_cmps(&x, &y, phaseDiff, channel);
                sum_x += x; // x=2^14, summed 6 times => sum_x maximum value 2^17
                sum_y += y;
            }
            int16_t bestFit_x = sum_x / 3; // bestfit maximum value 2^15 (reality is 2^14, but headroom is nice)
            int16_t bestFit_y = sum_y / 3;
            uint32_t residual = 0;
            for (uint8_t channel = 0; channel < 6; channel++)
            {
                q18_13 phaseDiff = signalPhaseDiffs[channel];
                if (channel == 2)
                    phaseDiff += 2 * qn_13_pi * ch2Wrap;
                if (channel == 3)
                    phaseDiff += 2 * qn_13_pi * ch3Wrap;
                int16_t x, y;
                get_radius_vector_cmps(&x, &y, phaseDiff, channel);
                int16_t diff_x = bestFit_x - x; // diff_x maximum 2^15
                int16_t diff_y = bestFit_y - y;
                q1_14 tangent_x = s_tangentVectors[channel][0];
                q1_14 tangent_y = s_tangentVectors[channel][1];
                int16_t dot_x = (int32_t)diff_x * tangent_x >> 14; // dot_x maximum 2^15
                int16_t dot_y = (int32_t)diff_y * tangent_y >> 14;
                uint32_t dist_2 = (int32_t)diff_x * diff_x + (int32_t)diff_y * diff_y -
                    ((int32_t)dot_x * dot_x + (int32_t)dot_y * dot_y); // maximum intermediate value 2^31.
                residual += dist_2 >> 2; // 2^31 summed 6 times but removed 2 bits => maximum 2^31.
            }
            if (residual < best_residual)
            {
                best_residual = residual;
                best_x = bestFit_x;
                best_y = bestFit_y;
            }
        }
    }
    *x_cmps = best_x;
    *y_cmps = best_y;
}

// Maximum returned values 2^14
void get_radius_vector_cmps(int16_t* x_cmps, int16_t* y_cmps, q18_13 phaseDiff, uint8_t channel)
{
    uint32_t phaseToTime_ns = 3900; //(uint32_t)(24.5 * 1000 / (2 * 3.141259));
    // Phase maximum value is +/- 3pi => 10.
    // delay_ns maximum value => +/- 40,000 (40us)
    q18_13 timeDiff_ns = phaseDiff * phaseToTime_ns;
    int32_t delay_ns = timeDiff_ns >> 13;
    uint16_t speed_cmps = get_speed_cmps(channel, delay_ns);
    *x_cmps = speed_cmps * (q17_14)s_radiusVectors[channel][0] >> 14;
    *y_cmps = speed_cmps * (q17_14)s_radiusVectors[channel][1] >> 14;
}

uint16_t g_temperature_degC_x10 = 250;
// Maximum windspeed 100m/s
// = 10,000 cm/s
// 14 bits
uint16_t get_speed_cmps(uint8_t channel, int32_t timeDiff_ns)
{
    uint32_t speedOfSound_dmps = 3315 + (g_temperature_degC_x10 * 61) / 100;
    uint32_t effective_length_um;
    if (channel == 2 || channel == 3)
        effective_length_um = 37000;
    else
        effective_length_um = 16000;
    // units: (dm * dm / um) / (s * s / ns) = (10^4m) / Gs? = 1E5 * m/s
    // So divide by 1000 to get cm/s
    // Formula is V = 1/2 * v_sound^2 * delta_T / length
    // Maximum time diff is 40,000
    // Maximum returned value is 16200 == 2^14
    return (speedOfSound_dmps * speedOfSound_dmps / effective_length_um) * timeDiff_ns / (2 * 1000);
}

void store_wind_sample(int16_t x_cmps, int16_t y_cmps)
{
    s_samples[s_sample_write_idx][0] = x_cmps;
    s_samples[s_sample_write_idx][1] = y_cmps;
    
    if (s_sample_write_idx == 0)
        s_sample_write_idx = SAMPLE_BUFFER_SIZE - 1;
    else
        s_sample_write_idx--;
}

void get_average_wind(int16_t *x_cmps, int16_t *y_cmps, uint8_t sampleCount)
{
    uint32_t sum_x = 0;
    uint32_t sum_y = 0;
    uint8_t end = s_sample_write_idx + 1 + sampleCount;
    for (uint8_t i = s_sample_write_idx + 1; i < end; i++)
    {
        sum_x += s_samples[i % SAMPLE_BUFFER_SIZE][0];
        sum_y += s_samples[i % SAMPLE_BUFFER_SIZE][1];
    }
    *x_cmps = sum_x / sampleCount;
    *y_cmps = sum_y / sampleCount;
}

uint16_t get_gust()
{
    uint32_t cur;
    uint32_t max = 0;
    uint8_t start = s_sample_write_idx == 0 ? SAMPLE_BUFFER_SIZE - 1 : s_sample_write_idx - 1;
    for (uint8_t i = start; i != s_sample_write_idx; 
        i = i == 0 ? SAMPLE_BUFFER_SIZE - 1 : i - 1)
    {
        uint16_t x = s_samples[i][0];
        uint16_t y = s_samples[i][1];
        uint32_t mag = sqrt(x * x + y * y); // Consider if we can remove this sqrt.
        // 4 sample exponential moving average
        cur = cur * 3 / 4 + mag;
        if (cur > max)
            max = cur;
    }
    return max / 4;
}

void get_wind_parameters(uint16_t* pAvg_dmps, uint16_t* pGust_dmps, uint16_t* pAngle_deg)
{
    int16_t x_cmps, y_cmps, gust_cmps;
    get_average_wind(&x_cmps, &y_cmps, 19);
    get_gust(&gust_cmps);
    *pAvg_dmps = sqrt(x_cmps * x_cmps + y_cmps * y_cmps) / 10;
    *pGust_dmps = gust_cmps / 10;
    double angle_rad = atan2(x_cmps, y_cmps);
    int16_t angle_deg = angle_rad * (180 / 3.141259);
    if (angle_deg < 0)
        angle_deg += 360;
    *pAngle_deg = angle_deg;
}