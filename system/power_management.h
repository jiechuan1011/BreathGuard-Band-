/*
 * power_management.h - 功耗管理模块头文件
 * 
 * 功能：深度睡眠模式、心率采样频率优化、OLED刷新率降低
 * 设计原则：
 * 1. 多级功耗模式
 * 2. 智能唤醒机制
 * 3. 外设功耗控制
 * 4. 电池保护
 */

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

/**
 * @brief 初始化功耗管理模块
 */
void power_mgmt_init();

/**
 * @brief 更新活动状态（用户活动时调用）
 */
void power_mgmt_update_activity();

/**
 * @brief 更新BLE连接状态
 * @param connected BLE连接状态
 */
void power_mgmt_update_ble_connection(bool connected);

/**
 * @brief 设置电池电量
 * @param level 电池电量百分比（0-100）
 */
void power_mgmt_set_battery_level(uint8_t level);

/**
 * @brief 功耗管理主任务（在主循环中调用）
 */
void power_mgmt_task();

/**
 * @brief 获取当前功耗模式
 * @return 当前功耗模式
 */
PowerMode power_mgmt_get_current_mode();

/**
 * @brief 检查是否低电量
 * @return true: 低电量, false: 电量正常
 */
bool power_mgmt_is_low_battery();

/**
 * @brief 进入深度睡眠模式
 * @param sleep_time_ms 睡眠时间（毫秒）
 */
void power_mgmt_enter_deep_sleep(uint32_t sleep_time_ms);

/**
 * @brief 进入轻睡眠模式
 */
void power_mgmt_enter_light_sleep();

/**
 * @brief 唤醒系统
 */
void power_mgmt_wakeup();

/**
 * @brief 设置传感器采样率
 * @param sample_rate_hz 采样率（Hz）
 */
void power_mgmt_set_sensor_sample_rate(uint16_t sample_rate_hz);

/**
 * @brief 设置OLED刷新率
 * @param refresh_rate_hz 刷新率（Hz），0表示关闭
 */
void power_mgmt_set_oled_refresh_rate(uint8_t refresh_rate_hz);

/**
 * @brief 设置BLE广播间隔
 * @param interval_ms 广播间隔（毫秒）
 */
void power_mgmt_set_ble_advertising_interval(uint16_t interval_ms);

/**
 * @brief 获取功耗统计信息
 * @param total_active_time 总活跃时间（毫秒）
 * @param total_sleep_time 总睡眠时间（毫秒）
 * @param battery_consumption 电池消耗（mAh）
 */
void power_mgmt_get_statistics(uint32_t* total_active_time, 
                               uint32_t* total_sleep_time,
                               float* battery_consumption);

/**
 * @brief 重置功耗统计
 */
void power_mgmt_reset_statistics();

/**
 * @brief 检查是否允许进入深度睡眠
 * @return true: 允许, false: 不允许
 */
bool power_mgmt_can_enter_deep_sleep();

/**
 * @brief 强制进入低功耗模式（用于紧急情况）
 */
void power_mgmt_force_low_power();

/**
 * @brief 恢复正常功耗模式
 */
void power_mgmt_resume_normal_power();

#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGEMENT_H