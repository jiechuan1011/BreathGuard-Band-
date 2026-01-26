#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>

// ──────────────────────────────────────────────
// 系统状态结构（全局共享数据，低RAM优化版本）
typedef struct {
    // 心率数据（低RAM优化：uint8_t代替float）
    uint8_t hr_bpm;              // 0=无效，40-180=BPM值
    uint8_t hr_snr_db_x10;      // SNR*10（例如15.3dB存储为153）
    int8_t hr_status;            // HR_SUCCESS, HR_POOR_SIGNAL 等（int8_t足够）
    uint16_t hr_timestamp_s;     // 时间戳（秒，节省2 bytes）

    // 气体数据（低RAM优化：uint16_t代替float）
    uint16_t gas_voltage_mv;     // 电压（mV，0-5000范围）
    uint16_t gas_concentration_ppm;  // 浓度（ppm，0-1000范围，精度0.1ppm用*10存储）
    bool gas_valid;
    uint16_t gas_timestamp_s;    // 时间戳（秒）

    // 环境数据（低RAM优化：int8_t+uint8_t代替float）
    int8_t env_temperature_c;    // 温度（℃，-40~85范围，int8_t足够）
    uint8_t env_humidity_rh;     // 湿度（%，0-100范围）
    bool env_valid;
    uint16_t env_timestamp_s;    // 时间戳（秒）

    // 测量窗口时间戳（低RAM优化：秒代替毫秒）
    uint16_t measurement_start_s;
    uint16_t measurement_end_s;
} SystemState;

// ──────────────────────────────────────────────
// 函数声明
void system_state_init();                                    // 初始化
void system_state_reset_measurement();                       // 清空测量数据（开始新窗口）
void system_state_set_hr(uint8_t bpm, uint8_t snr_x10, int8_t status);  // 更新心率
void system_state_set_gas(uint16_t voltage_mv, uint16_t conc_ppm_x10, bool valid);  // 更新气体
void system_state_set_env(int8_t temp_c, uint8_t rh, bool valid);            // 更新环境
const SystemState* system_state_get();                       // 获取当前状态（只读）

#endif
