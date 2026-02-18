#ifndef ENV_DRIVER_H
#define ENV_DRIVER_H

#include <Arduino.h>
#include "config/pin_config.h"

typedef struct {
    float temperature_c;
    float humidity_rh;
    bool valid;
} EnvData;

bool env_init();
bool env_read(EnvData* out);

#endif
