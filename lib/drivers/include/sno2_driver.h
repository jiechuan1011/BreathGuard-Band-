#ifndef SNO2_DRIVER_H
#define SNO2_DRIVER_H

#include <Arduino.h>
#include "config/pin_config.h"

// Copy of drivers/sno2_driver.h content (shortened for brevity)
// Full header moved here to lib include so LDF can find it.

typedef enum {
    SNO2_STATE_IDLE = 0,
    SNO2_STATE_HEATING,
    SNO2_STATE_SAMPLING,
    SNO2_STATE_CALCULATING,
    SNO2_STATE_ERROR
} Sno2State;

typedef struct {
    uint16_t voltage_mv;
    uint16_t concentration_ppm;
    uint8_t valid : 1;
    uint8_t heater_on : 1;
    uint8_t reserved : 6;
} Sno2Data;

void sno2_init();
void sno2_update();
Sno2State sno2_get_state();
Sno2Data sno2_get_data();
void sno2_set_calibration(float a, float b);
uint8_t sno2_is_heater_on();
uint32_t sno2_get_heating_remaining();
uint32_t sno2_get_next_sample_time();

#endif // SNO2_DRIVER_H
