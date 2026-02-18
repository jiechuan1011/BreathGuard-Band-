#ifndef GAS_DRIVER_H
#define GAS_DRIVER_H

#include <Arduino.h>
#include "config/pin_config.h"

// Minimal copy of gas_driver.h so LDF finds it

bool gas_init();
bool gas_read(float* voltage_mv, float* conc_ppm);
bool gas_is_warmed_up();
float gas_get_heater_duty_cycle();
uint32_t gas_get_warmup_remaining();

#endif
