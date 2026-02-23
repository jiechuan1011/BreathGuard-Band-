// Copied from system/power_management.h for LDF
#ifndef POWER_MANAGEMENT_H
#define POWER_MANAGEMENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 功耗模式定义 ====================
typedef enum {
    POWER_MODE_ACTIVE = 0,      // 活跃模式：全功能运行
    POWER_MODE_LOW_POWER,       // 低功耗模式：限制外设
    POWER_MODE_SLEEP,           // 睡眠模式：仅基本功能
    POWER_MODE_DEEP_SLEEP       // 深度睡眠：最低功耗
} PowerMode;

// ==================== 功耗配置结构 ====================
typedef struct {
    uint32_t active_mode_duration;      // 活跃模式持续时间（ms）
    uint8_t low_power_threshold;        // 低电量阈值（%）
    uint32_t deep_sleep_timeout;        // 深度睡眠超时时间（ms）
    
    uint16_t heart_rate_sample_active;  // 活跃模式心率采样率（Hz）
    uint16_t heart_rate_sample_low_power; // 低功耗模式心率采样率（Hz）
    uint16_t heart_rate_sample_sleep;   // 睡眠模式心率采样率（Hz）
    
    uint8_t oled_refresh_active;        // 活跃模式OLED刷新率（Hz）
    uint8_t oled_refresh_low_power;     // 低功耗模式OLED刷新率（Hz）
    uint8_t oled_refresh_sleep;         // 睡眠模式OLED刷新率（Hz）
    
    uint16_t ble_adv_interval_active;   // 活跃模式BLE广播间隔（ms）
    uint16_t ble_adv_interval_low_power; // 低功耗模式BLE广播间隔（ms）
    
    uint32_t cpu_frequency_active;      // 活跃模式CPU频率（MHz）
    uint32_t cpu_frequency_low_power;   // 低功耗模式CPU频率（MHz）
    uint32_t cpu_frequency_sleep;       // 睡眠模式CPU频率（MHz）
} PowerConfig;

// ==================== 公共接口函数 ====================

void power_mgmt_init();
void power_mgmt_update_activity();
void power_mgmt_update_ble_connection(bool connected);
void power_mgmt_set_battery_level(uint8_t level);
void power_mgmt_task();
PowerMode power_mgmt_get_current_mode();
bool power_mgmt_is_low_battery();
void power_mgmt_enter_deep_sleep(uint32_t sleep_time_ms);
void power_mgmt_enter_light_sleep();
void power_mgmt_wakeup();
void power_mgmt_set_sensor_sample_rate(uint16_t sample_rate_hz);
void power_mgmt_set_oled_refresh_rate(uint8_t refresh_rate_hz);
void power_mgmt_set_ble_advertising_interval(uint16_t interval_ms);
void power_mgmt_get_statistics(uint32_t* total_active_time, 
                               uint32_t* total_sleep_time,
                               float* battery_consumption);
void power_mgmt_reset_statistics();
bool power_mgmt_can_enter_deep_sleep();
void power_mgmt_force_low_power();
void power_mgmt_resume_normal_power();

#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGEMENT_H
