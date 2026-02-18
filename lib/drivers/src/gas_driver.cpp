#include "gas_driver.h"
#include <math.h>

static unsigned long warmup_start_ms = 0;
static bool warmup_complete = false;
static uint8_t heater_duty_cycle = 0;

bool gas_init() {
    warmup_start_ms = millis();
    warmup_complete = true;
    heater_duty_cycle = 0;
    return true;
}

bool gas_read(float* voltage_mv, float* conc_ppm) {
    *voltage_mv = 0.0;
    *conc_ppm = 0.0;
    return true;
}

bool gas_is_warmed_up() { return warmup_complete; }
float gas_get_heater_duty_cycle() { return heater_duty_cycle; }
uint32_t gas_get_warmup_remaining() { return 0; }
