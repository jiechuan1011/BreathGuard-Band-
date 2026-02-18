#include "env_driver.h"
#include <Wire.h>

bool env_init() { return true; }
bool env_read(EnvData* out) { if (!out) return false; out->temperature_c=25.0; out->humidity_rh=50.0; out->valid=true; return true; }
