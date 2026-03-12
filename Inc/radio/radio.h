#pragma once

#include <stdbool.h>

extern const uint8_t radio_config[];
extern const uint8_t radio_data_rate_bank[];

void configure_radio(bool first_init,uint16_t frequency_selector);

#if 0
enum radio_state_enum
{
    sleep = 1,
    stby = 2,
    rfs = 3,
    tx = 4,
    rx = 5,
    tfs = 6
} typedef radio_state;

enum radio_state_cmd_enum
{
    go_stby = 2,
    go_rfs = 4,
    go_rx = 8,
    go_sleep = 0x10,
    go_tfs = 0x20,
    go_tx = 0x40
} typedef radio_state_cmd;
 #endif