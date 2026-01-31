/*
 * power_management.cpp - 功耗管理模块
 * 
 * 功能：深度睡眠模式、心率采样频率优化、OLED刷新率降低
 * 设计原则：
 * 1. 多级功耗模式
 * 2. 智能唤醒机制
 * 3. 外设功耗控制
 * 4. 电池保护
 */

#include "power_management.h"
#include "../config/config.h"
#include "../config/pin_config.h"
#include <Arduino.h>

#ifdef ESP32
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <driver/adc.h>
#endif

// ==================== 功耗模式定义 ====================
typedef enum {
    POWER_MODE_ACTIVE = 0,      // 活跃模式：全功能运行
    POWER_MODE_LOW_POWER,       // 低功耗模式：限制外设
    POWER_MODE_SLEEP,           // 睡眠模式：仅基本功能
    POWER_MODE_DEEP_SLEEP       // 深度睡眠：最低功耗
} PowerMode;

// ==================== 系统状态变量 ====================
static struct {
    PowerMode current_mode;
    PowerMode target_mode;
    uint32_t last_activity_time;
    uint32_t sleep_start_time;
    uint8_t battery_level;
    bool ble_connected;
    bool user_active;
    bool low_battery;
    uint8_t error_count;
} power_state = {
    .current_mode = POWER_MODE_ACTIVE,
    .target_mode = POWER_MODE_ACTIVE,
    .last_activity_time = 0,
    .sleep_start_time = 0,
    .battery_level = 100,
    .ble_connected = false,
    .user_active = true,
    .low_battery = false,
    .error_count = 0
};

// ==================== 功耗配置参数 ====================
static const PowerConfig power_config = {
    .active_mode_duration = 30000,      // 30秒活跃时间
    .low_power_threshold = 30,          // 30%电量进入低功耗
    .deep_sleep_timeout = 60000,        // 60秒无活动进入深度睡眠
    .heart_rate_sample_active = 100,    // 活跃模式：100Hz采样
    .heart_rate_sample_low_power = 50,  // 低功耗模式：50Hz采样
    .heart_rate_sample_sleep = 10,      // 睡眠模式：10Hz采样
    .oled_refresh_active = 2,           // 活跃模式：2Hz刷新
    .oled_refresh_low_power = 1,        // 低功耗模式：1Hz刷新
    .oled_refresh_sleep = 0,            // 睡眠模式：关闭刷新
    .ble_adv_interval_active = 800,     // 活跃模式：800ms广播间隔
    .ble_adv_interval_low_power = 1600, // 低功耗模式：1600ms广播间隔
    .cpu_frequency_active = 240,        // 活跃模式：240MHz
    .cpu_frequency_low_power = 80,      // 低功耗模式：80MHz
    .cpu_frequency_sleep = 10           // 睡眠模式：10MHz（如果支持）
};

// ==================== 初始化函数 ====================
void power_management_init() {
    // 初始化功耗管理状态
    power_state.current_mode = POWER_MODE_ACTIVE;
    power_state.target_mode = POWER_MODE_ACTIVE;
    power_state.last_activity_time = millis();
    power_state.battery_level = 100;
    power_state.ble_connected = false;
    power_state.user_active = true;
    power_state.low_battery = false;
    power_state.error_count = 0;
    
    // 配置唤醒源
#ifdef ESP32
    // 配置GPIO唤醒源
    setup_wakeup_sources();
#endif
    
    Serial.println("[功耗] 功耗管理初始化完成");
}

// ==================== 功耗模式切换 ====================
void set_power_mode(PowerMode mode) {
    if (mode == power_state.current_mode) {
        return; // 已经是目标模式
    }
    
    Serial.printf("[功耗] 切换功耗模式: %d -> %d\n", 
                  power_state.current_mode, mode);
    
    // 退出当前模式
    exit_power_mode(power_state.current_mode);
    
    // 进入新模式
    enter_power_mode(mode);
    
    power_state.current_mode = mode;
}

// ==================== 退出功耗模式 ====================
void exit_power_mode(PowerMode mode) {
    switch (mode) {
        case POWER_MODE_DEEP_SLEEP:
            // 深度睡眠唤醒后的处理
            handle_deep_sleep_wakeup();
            break;
            
        case POWER_MODE_SLEEP:
            // 睡眠模式唤醒
            handle_sleep_wakeup();
            break;
            
        case POWER_MODE_LOW_POWER:
            // 低功耗模式退出
            break;
            
        case POWER_MODE_ACTIVE:
            // 活跃模式退出
            break;
    }
}

// ==================== 进入功耗模式 ====================
void enter_power_mode(PowerMode mode) {
    switch (mode) {
        case POWER_MODE_ACTIVE:
            enter_active_mode();
            break;
            
        case POWER_MODE_LOW_POWER:
            enter_low_power_mode();
            break;
            
        case POWER_MODE_SLEEP:
            enter_sleep_mode();
            break;
            
        case POWER_MODE_DEEP_SLEEP:
            enter_deep_sleep_mode();
            break;
    }
}

// ==================== 活跃模式 ====================
void enter_active_mode() {
    Serial.println("[功耗] 进入活跃模式");
    
    // 设置CPU频率
#ifdef ESP32
    setCpuFrequencyMhz(power_config.cpu_frequency_active);
#endif
    
    // 启用所有外设
    enable_peripherals(true);
    
    // 设置传感器采样率
    set_sensor_sample_rate(power_config.heart_rate_sample_active);
    
    // 设置OLED刷新率
    set_oled_refresh_rate(power_config.oled_refresh_active);
    
    // 设置BLE广播间隔
    set_ble_advertising_interval(power_config.ble_adv_interval_active);
}

// ==================== 低功耗模式 ====================
void enter_low_power_mode() {
    Serial.println("[功耗] 进入低功耗模式");
    
    // 降低CPU频率
#ifdef ESP32
    setCpuFrequencyMhz(power_config.cpu_frequency_low_power);
#endif
    
    // 限制外设功耗
    enable_peripherals(false);
    
    // 降低传感器采样率
    set_sensor_sample_rate(power_config.heart_rate_sample_low_power);
    
    // 降低OLED刷新率
    set_oled_refresh_rate(power_config.oled_refresh_low_power);
    
    // 增加BLE广播间隔
    set_ble_advertising_interval(power_config.ble_adv_interval_low_power);
}

// ==================== 睡眠模式 ====================
void enter_sleep_mode() {
    Serial.println("[功耗] 进入睡眠模式");
    
    // 进一步降低CPU频率
#ifdef ESP32
    setCpuFrequencyMhz(power_config.cpu_frequency_sleep);
#endif
    
    // 关闭非必要外设
    disable_non_essential_peripherals();
    
    // 极低传感器采样率
    set_sensor_sample_rate(power_config.heart_rate_sample_sleep);
    
    // 关闭OLED显示
    set_oled_refresh_rate(power_config.oled_refresh_sleep);
    
    // 记录睡眠开始时间
    power_state.sleep_start_time = millis();
}

// ==================== 深度睡眠模式 ====================
void enter_deep_sleep_mode() {
    Serial.println("[功耗] 进入深度睡眠模式");
    
    // 保存系统状态
    save_system_state();
    
    // 关闭所有外设
    disable_all_peripherals();
    
    // 配置唤醒源
    configure_wakeup_sources();
    
    // 进入深度睡眠
#ifdef ESP32
    Serial.printf("[功耗] 深度睡眠 %lu ms\n", power_config.deep_sleep_timeout);
    esp_sleep_enable_timer_wakeup(power_config.deep_sleep_timeout * 1000);
    esp_deep_sleep_start();
#endif
}

// ==================== 唤醒处理 ====================
void handle_deep_sleep_wakeup() {
    Serial.println("[功耗] 从深度睡眠唤醒");
    
    // 恢复系统状态
    restore_system_state();
    
    // 更新活动时间
    power_state.last_activity_time = millis();
    power_state.user_active = true;
}

void handle_sleep_wakeup() {
    Serial.println("[功耗] 从睡眠模式唤醒");
    
    // 更新活动时间
    power_state.last_activity_time = millis();
    power_state.user_active = true;
}

// ==================== 外设控制 ====================
void enable_peripherals(bool full_power) {
    // 控制外设电源
    if (full_power) {
        // 启用所有外设
        Serial.println("[功耗] 启用所有外设");
    } else {
        // 仅启用必要外设
        Serial.println("[功耗] 启用必要外设");
    }
}

void disable_non_essential_peripherals() {
    Serial.println("[功耗] 关闭非必要外设");
    
    // 关闭OLED显示
    // oled_power_off();
    
    // 降低传感器功耗
    // sensor_low_power_mode();
}

void disable_all_peripherals() {
    Serial.println("[功耗] 关闭所有外设");
    
    // 关闭所有外设电源
    // 这里实现具体的外设关闭逻辑
}

// ==================== 传感器采样率控制 ====================
void set_sensor_sample_rate(uint16_t sample_rate_hz) {
    Serial.printf("[功耗] 设置传感器采样率: %d Hz\n", sample_rate_hz);
    
    // 这里实现传感器采样率控制
    // 例如：配置MAX30102采样率
}

// ==================== OLED刷新率控制 ====================
void set_oled_refresh_rate(uint8_t refresh_rate_hz) {
    Serial.printf("[功耗] 设置OLED刷新率: %d Hz\n", refresh_rate_hz);
    
    if (refresh_rate_hz == 0) {
        // 关闭OLED
        // oled_power_off();
    } else {
        // 设置OLED刷新率
        // oled_set_refresh_rate(refresh_rate_hz);
    }
}

// ==================== BLE广播间隔控制 ====================
void set_ble_advertising_interval(uint16_t interval_ms) {
    Serial.printf("[功耗] 设置BLE广播间隔: %d ms\n", interval_ms);
    
    // 这里实现BLE广播间隔控制
}

// ==================== 唤醒源配置 ====================
void setup_wakeup_sources() {
#ifdef ESP32
    // 配置GPIO唤醒
    // 例如：按键唤醒
#ifdef PIN_BTN1
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN1, 0); // 低电平唤醒
#endif
    
    // 配置定时器唤醒
    esp_sleep_enable_timer_wakeup(power_config.deep_sleep_timeout * 1000);
#endif
}

void configure_wakeup_sources() {
    // 配置深度睡眠唤醒源
#ifdef ESP32
    // 可以在这里动态配置唤醒源
#endif
}

// ==================== 系统状态保存/恢复 ====================
void save_system_state() {
    Serial.println("[功耗] 保存系统状态");
    
    // 保存当前系统状态到RTC内存或Flash
    // 这里实现状态保存逻辑
}

void restore_system_state() {
    Serial.println("[功耗] 恢复系统状态");
    
    // 从RTC内存或Flash恢复系统状态
    // 这里实现状态恢复逻辑
}

// ==================== 电池管理 ====================
void update_battery_level(uint8_t level) {
    power_state.battery_level = level;
    
    // 检查低电量
    if (level <= power_config.low_power_threshold) {
        power_state.low_battery = true;
        
        // 低电量保护
        if (level <= 10) {
            Serial.println("[警告] 电池电量极低，即将关机");
            // 进入深度睡眠保护
            set_power_mode(POWER_MODE_DEEP_SLEEP);
        } else if (level <= 20) {
            Serial.println("[警告] 电池电量低，进入省电模式");
            // 进入低功耗模式
            set_power_mode(POWER_MODE_LOW_POWER);
        }
    } else {
        power_state.low_battery = false;
    }
}

// ==================== 活动检测 ====================
void update_activity() {
    power_state.last_activity_time = millis();
    power_state.user_active = true;
    
    // 如果有活动，切换到活跃模式
    if (power_state.current_mode != POWER_MODE_ACTIVE) {
        set_power_mode(POWER_MODE_ACTIVE);
    }
}

// ==================== BLE连接状态更新 ====================
void update_ble_connection(bool connected) {
    power_state.ble_connected = connected;
    
    if (connected) {
        // BLE连接时保持活跃
        update_activity();
    }
}

// ==================== 主循环功耗管理 ====================
void power_management_task() {
    uint32_t current_time = millis();
    uint32_t inactive_time = current_time - power_state.last_activity_time;
    
    // 检查是否需要切换功耗模式
    check_power_mode_transition(inactive_time);
    
    // 检查电池状态
    check_battery_status();
    
    // 执行当前模式的功耗优化
    execute_power_optimization();
}

// ==================== 功耗模式转换检查 ====================
void check_power_mode_transition(uint32_t inactive_time) {
    // 基于不活动时间决定目标模式
    if (power_state.ble_connected || power_state.user_active) {
        power_state.target_mode = POWER_MODE_ACTIVE;
    } else if (inactive_time > power_config.deep_sleep_timeout) {
        power_state.target_mode = POWER_MODE_DEEP_SLEEP;
    } else if (inactive_time > power_config.active_mode_duration) {
        power_state.target_mode = POWER_MODE_LOW_POWER;
    } else {
        power_state.target_mode = POWER_MODE_ACTIVE;
    }
    
    // 检查是否需要切换模式
    if (power_state.target_mode != power_state.current_mode) {
        set_power_mode(power_state.target_mode);
    }
}

// ==================== 电池状态检查 ====================
void check_battery_status() {
    // 这里可以添加电池电压读取和状态更新
    // update_battery_level(read_battery_level());
}

// ==================== 功耗优化执行 ====================
void execute_power_optimization() {
    switch (power_state.current_mode) {
        case POWER_MODE_ACTIVE:
            // 活跃模式：最小化优化
            break;
            
        case POWER_MODE_LOW_POWER:
            // 低功耗模式：中等优化
            optimize_for_low_power();
            break;
            
        case POWER_MODE_SLEEP:
            // 睡眠模式：最大优化
            optimize_for_sleep();
            break;
            
        case POWER_MODE_DEEP_SLEEP:
            // 深度睡眠：已进入睡眠，无需优化
            break;
    }
}

// ==================== 低功耗优化 ====================
void optimize_for_low_power() {
    // 进一步降低非关键任务的频率
    // 例如：减少数据记录频率
}

// ==================== 睡眠优化 ====================
void optimize_for_sleep() {
    // 最大化功耗优化
    // 例如：关闭所有非必要功能
}

// ==================== 公共接口函数 ====================
#ifdef __cplusplus
extern "C" {
#endif

void power_mgmt_init() {
    power_management_init();
}

void power_mgmt_update_activity() {
    update_activity();
}

void power_mgmt_update_ble_connection(bool connected) {
    update_ble_connection(connected);
}

void power_mgmt_set_battery_level(uint8_t level) {
    update_battery_level(level);
}

void power_mgmt_task() {
    power_management_task();
}

PowerMode power_mgmt_get_current_mode() {
    return power_state.current_mode;
}

bool power_mgmt_is_low_battery() {
    return power_state.low_battery;
}

#ifdef __cplusplus
}
#endif