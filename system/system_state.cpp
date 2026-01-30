#include "system_state.h"
#include <Arduino.h>
#include <string.h>  // for memset
#include "../algorithm/hr_algorithm.h"  // for HR_SUCCESS等常量

static SystemState g_state;

void system_state_init() {
    memset(&g_state, 0, sizeof(g_state));
    g_state.hr_bpm = 0;  // 0表示无效
    g_state.hr_status = -1;
    g_state.gas_valid = false;
    g_state.env_valid = false;
}

void system_state_reset_measurement() {
    // 清空测量数据，但保留时间戳用于判断
    g_state.hr_bpm = 0;
    g_state.hr_snr_db_x10 = 0;
    g_state.hr_status = -1;
    g_state.gas_voltage_mv = 0;
    g_state.gas_concentration_ppm = 0;
    g_state.gas_valid = false;
    g_state.env_temperature_c = 0;
    g_state.env_humidity_rh = 0;
    g_state.env_valid = false;
    g_state.measurement_start_s = (uint16_t)(millis() / 1000);
    g_state.measurement_end_s = 0;
}

void system_state_set_hr(uint8_t bpm, uint8_t snr_x10, int8_t status) {
    g_state.hr_bpm = bpm;
    g_state.hr_snr_db_x10 = snr_x10;
    g_state.hr_status = status;
    g_state.hr_timestamp_s = (uint16_t)(millis() / 1000);
}

#ifdef DEVICE_ROLE_WRIST
void system_state_set_hr_spo2(uint8_t bpm, uint8_t spo2, uint8_t snr_x10, uint8_t correlation, int8_t status) {
    g_state.hr_bpm = bpm;
    g_state.spo2_value = spo2;
    g_state.hr_snr_db_x10 = snr_x10;
    g_state.correlation_quality = correlation;
    g_state.hr_status = status;
    g_state.hr_timestamp_s = (uint16_t)(millis() / 1000);
}

uint8_t system_state_get_spo2() {
    return g_state.spo2_value;
}

uint8_t system_state_get_correlation_quality() {
    return g_state.correlation_quality;
}
#endif

void system_state_set_gas(uint16_t voltage_mv, uint16_t conc_ppm_x10, bool valid) {
    g_state.gas_voltage_mv = voltage_mv;
    g_state.gas_concentration_ppm = conc_ppm_x10;
    g_state.gas_valid = valid;
    g_state.gas_timestamp_s = (uint16_t)(millis() / 1000);
}

void system_state_set_env(int8_t temp_c, uint8_t rh, bool valid) {
    g_state.env_temperature_c = temp_c;
    g_state.env_humidity_rh = rh;
    g_state.env_valid = valid;
    g_state.env_timestamp_s = (uint16_t)(millis() / 1000);
}

const SystemState* system_state_get() {
    return &g_state;
}
