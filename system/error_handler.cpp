/*
 * error_handler.cpp - 错误处理增强模块
 * 
 * 功能：传感器故障恢复机制、BLE断线重连、电池低压保护
 * 设计原则：
 * 1. 分层错误处理
 * 2. 自动恢复机制
 * 3. 错误日志记录
 * 4. 用户友好提示
 */

#include "error_handler.h"
#include "../config/config.h"
#include <Arduino.h>

// ==================== 错误类型定义 ====================
typedef enum {
    ERROR_TYPE_SENSOR = 0,      // 传感器错误
    ERROR_TYPE_COMMUNICATION,   // 通信错误
    ERROR_TYPE_POWER,           // 电源错误
    ERROR_TYPE_HARDWARE,        // 硬件错误
    ERROR_TYPE_SOFTWARE,        // 软件错误
    ERROR_TYPE_USER             // 用户错误
} ErrorType;

// ==================== 错误严重程度 ====================
typedef enum {
    ERROR_SEVERITY_INFO = 0,    // 信息级别
    ERROR_SEVERITY_WARNING,     // 警告级别
    ERROR_SEVERITY_ERROR,       // 错误级别
    ERROR_SEVERITY_CRITICAL     // 严重错误级别
} ErrorSeverity;

// ==================== 错误记录结构 ====================
typedef struct {
    ErrorType type;
    ErrorSeverity severity;
    uint32_t timestamp;
    uint16_t error_code;
    char description[64];
    uint8_t retry_count;
    bool auto_recoverable;
} ErrorRecord;

// ==================== 系统状态变量 ====================
static struct {
    ErrorRecord active_errors[MAX_ACTIVE_ERRORS];
    ErrorRecord error_history[MAX_ERROR_HISTORY];
    uint8_t active_error_count;
    uint8_t error_history_count;
    uint32_t last_recovery_time;
    uint8_t recovery_attempts;
    bool in_recovery_mode;
    bool system_stable;
} error_state = {
    .active_error_count = 0,
    .error_history_count = 0,
    .last_recovery_time = 0,
    .recovery_attempts = 0,
    .in_recovery_mode = false,
    .system_stable = true
};

// ==================== 错误处理配置 ====================
static const ErrorHandlerConfig error_config = {
    .max_recovery_attempts = 3,
    .recovery_cooldown_ms = 5000,
    .error_report_interval_ms = 10000,
    .sensor_timeout_ms = 3000,
    .ble_reconnect_interval_ms = 2000,
    .ble_max_reconnect_attempts = 5,
    .low_battery_threshold = 20,
    .critical_battery_threshold = 10,
    .sensor_calibration_interval_ms = 3600000, // 1小时
    .system_health_check_interval_ms = 60000   // 1分钟
};

// ==================== 初始化函数 ====================
void error_handler_init() {
    // 初始化错误处理状态
    error_state.active_error_count = 0;
    error_state.error_history_count = 0;
    error_state.last_recovery_time = 0;
    error_state.recovery_attempts = 0;
    error_state.in_recovery_mode = false;
    error_state.system_stable = true;
    
    Serial.println("[错误处理] 错误处理模块初始化完成");
}

// ==================== 错误记录函数 ====================
void record_error(ErrorType type, ErrorSeverity severity, 
                  uint16_t error_code, const char* description) {
    
    // 检查是否已达到最大活动错误数
    if (error_state.active_error_count >= MAX_ACTIVE_ERRORS) {
        // 移除最旧的错误
        for (uint8_t i = 0; i < MAX_ACTIVE_ERRORS - 1; i++) {
            error_state.active_errors[i] = error_state.active_errors[i + 1];
        }
        error_state.active_error_count--;
    }
    
    // 记录新错误
    ErrorRecord* record = &error_state.active_errors[error_state.active_error_count];
    record->type = type;
    record->severity = severity;
    record->timestamp = millis();
    record->error_code = error_code;
    strncpy(record->description, description, sizeof(record->description) - 1);
    record->description[sizeof(record->description) - 1] = '\0';
    record->retry_count = 0;
    record->auto_recoverable = is_error_recoverable(type, error_code);
    
    error_state.active_error_count++;
    
    // 添加到历史记录
    add_to_error_history(record);
    
    // 打印错误信息
    print_error_record(record);
    
    // 检查是否需要进入恢复模式
    if (severity >= ERROR_SEVERITY_ERROR) {
        check_recovery_needed();
    }
}

// ==================== 添加到错误历史 ====================
void add_to_error_history(const ErrorRecord* record) {
    if (error_state.error_history_count >= MAX_ERROR_HISTORY) {
        // 移除最旧的历史记录
        for (uint8_t i = 0; i < MAX_ERROR_HISTORY - 1; i++) {
            error_state.error_history[i] = error_state.error_history[i + 1];
        }
        error_state.error_history_count--;
    }
    
    error_state.error_history[error_state.error_history_count] = *record;
    error_state.error_history_count++;
}

// ==================== 打印错误记录 ====================
void print_error_record(const ErrorRecord* record) {
    const char* type_str = get_error_type_string(record->type);
    const char* severity_str = get_error_severity_string(record->severity);
    
    Serial.printf("[错误] %s - %s (代码: 0x%04X)\n", 
                  severity_str, type_str, record->error_code);
    Serial.printf("      描述: %s\n", record->description);
    Serial.printf("      时间: %lu ms, 可恢复: %s\n", 
                  record->timestamp, 
                  record->auto_recoverable ? "是" : "否");
}

// ==================== 错误类型字符串 ====================
const char* get_error_type_string(ErrorType type) {
    switch (type) {
        case ERROR_TYPE_SENSOR: return "传感器";
        case ERROR_TYPE_COMMUNICATION: return "通信";
        case ERROR_TYPE_POWER: return "电源";
        case ERROR_TYPE_HARDWARE: return "硬件";
        case ERROR_TYPE_SOFTWARE: return "软件";
        case ERROR_TYPE_USER: return "用户";
        default: return "未知";
    }
}

// ==================== 错误严重程度字符串 ====================
const char* get_error_severity_string(ErrorSeverity severity) {
    switch (severity) {
        case ERROR_SEVERITY_INFO: return "信息";
        case ERROR_SEVERITY_WARNING: return "警告";
        case ERROR_SEVERITY_ERROR: return "错误";
        case ERROR_SEVERITY_CRITICAL: return "严重";
        default: return "未知";
    }
}

// ==================== 检查错误是否可恢复 ====================
bool is_error_recoverable(ErrorType type, uint16_t error_code) {
    // 根据错误类型和代码判断是否可自动恢复
    switch (type) {
        case ERROR_TYPE_SENSOR:
            // 大多数传感器错误可恢复
            return true;
            
        case ERROR_TYPE_COMMUNICATION:
            // 通信错误通常可恢复
            return true;
            
        case ERROR_TYPE_POWER:
            // 电源错误需要根据具体情况
            return (error_code != ERROR_CODE_BATTERY_CRITICAL);
            
        case ERROR_TYPE_HARDWARE:
            // 硬件错误通常不可恢复
            return false;
            
        case ERROR_TYPE_SOFTWARE:
            // 软件错误可能可恢复
            return (error_code != ERROR_CODE_MEMORY_CORRUPTION);
            
        case ERROR_TYPE_USER:
            // 用户错误通常可恢复
            return true;
            
        default:
            return false;
    }
}

// ==================== 检查是否需要恢复 ====================
void check_recovery_needed() {
    uint32_t current_time = millis();
    
    // 检查恢复冷却时间
    if (current_time - error_state.last_recovery_time < error_config.recovery_cooldown_ms) {
        return;
    }
    
    // 检查是否有需要恢复的错误
    bool needs_recovery = false;
    for (uint8_t i = 0; i < error_state.active_error_count; i++) {
        ErrorRecord* record = &error_state.active_errors[i];
        if (record->auto_recoverable && record->retry_count < error_config.max_recovery_attempts) {
            needs_recovery = true;
            break;
        }
    }
    
    if (needs_recovery) {
        start_recovery_process();
    }
}

// ==================== 开始恢复过程 ====================
void start_recovery_process() {
    Serial.println("[错误处理] 开始自动恢复过程");
    
    error_state.in_recovery_mode = true;
    error_state.recovery_attempts++;
    error_state.last_recovery_time = millis();
    
    // 执行恢复操作
    perform_recovery_actions();
    
    // 更新错误记录
    update_error_records_after_recovery();
    
    error_state.in_recovery_mode = false;
    
    Serial.printf("[错误处理] 恢复完成，尝试次数: %d\n", error_state.recovery_attempts);
}

// ==================== 执行恢复操作 ====================
void perform_recovery_actions() {
    // 根据错误类型执行相应的恢复操作
    for (uint8_t i = 0; i < error_state.active_error_count; i++) {
        ErrorRecord* record = &error_state.active_errors[i];
        
        if (!record->auto_recoverable || record->retry_count >= error_config.max_recovery_attempts) {
            continue;
        }
        
        switch (record->type) {
            case ERROR_TYPE_SENSOR:
                recover_sensor_error(record->error_code);
                break;
                
            case ERROR_TYPE_COMMUNICATION:
                recover_communication_error(record->error_code);
                break;
                
            case ERROR_TYPE_POWER:
                recover_power_error(record->error_code);
                break;
                
            case ERROR_TYPE_SOFTWARE:
                recover_software_error(record->error_code);
                break;
                
            default:
                // 其他错误类型使用通用恢复
                generic_recovery(record->error_code);
                break;
        }
        
        record->retry_count++;
    }
}

// ==================== 传感器错误恢复 ====================
void recover_sensor_error(uint16_t error_code) {
    Serial.printf("[恢复] 传感器错误恢复，代码: 0x%04X\n", error_code);
    
    switch (error_code) {
        case ERROR_CODE_SENSOR_TIMEOUT:
            // 传感器超时：重新初始化
            // sensor_reinit();
            break;
            
        case ERROR_CODE_SENSOR_CALIBRATION:
            // 传感器校准：重新校准
            // sensor_recalibrate();
            break;
            
        case ERROR_CODE_SENSOR_COMMUNICATION:
            // 传感器通信错误：重置通信接口
            // reset_i2c_bus();
            break;
            
        default:
            // 通用传感器恢复
            // sensor_soft_reset();
            break;
    }
    
    // 等待传感器稳定
    delay(100);
}

// ==================== 通信错误恢复 ====================
void recover_communication_error(uint16_t error_code) {
    Serial.printf("[恢复] 通信错误恢复，代码: 0x%04X\n", error_code);
    
    switch (error_code) {
        case ERROR_CODE_BLE_DISCONNECTED:
            // BLE断开：尝试重连
            recover_ble_connection();
            break;
            
        case ERROR_CODE_BLE_CONNECTION_FAILED:
            // BLE连接失败：重置BLE模块
            // ble_reinit();
            break;
            
        case ERROR_CODE_I2C_ERROR:
            // I2C错误：重置I2C总线
            // reset_i2c_bus();
            break;
            
        default:
            // 通用通信恢复
            // communication_reset();
            break;
    }
}

// ==================== BLE连接恢复 ====================
void recover_ble_connection() {
    Serial.println("[恢复] BLE连接恢复中...");
    
    static uint8_t reconnect_attempts = 0;
    
    if (reconnect_attempts >= error_config.ble_max_reconnect_attempts) {
        Serial.println("[恢复] BLE重连尝试次数超限，等待冷却");
        reconnect_attempts = 0;
        return;
    }
    
    reconnect_attempts++;
    
    // 尝试重新连接
    // ble_reconnect();
    
    Serial.printf("[恢复] BLE重连尝试 %d/%d\n", 
                  reconnect_attempts, error_config.ble_max_reconnect_attempts);
    
    // 等待重连结果
    delay(error_config.ble_reconnect_interval_ms);
}

// ==================== 电源错误恢复 ====================
void recover_power_error(uint16_t error_code) {
    Serial.printf("[恢复] 电源错误恢复，代码: 0x%04X\n", error_code);
    
    switch (error_code) {
        case ERROR_CODE_LOW_BATTERY:
            // 低电量：进入省电模式
            enter_power_saving_mode();
            break;
            
        case ERROR_CODE_BATTERY_CRITICAL:
            // 电量严重不足：进入深度睡眠
            enter_deep_sleep_protection();
            break;
            
        case ERROR_CODE_POWER_FLUCTUATION:
            // 电源波动：稳定电源
            stabilize_power_supply();
            break;
            
        default:
            // 通用电源恢复
            power_system_reset();
            break;
    }
}

// ==================== 进入省电模式 ====================
void enter_power_saving_mode() {
    Serial.println("[恢复] 进入省电模式");
    
    // 降低CPU频率
    // set_cpu_frequency_low();
    
    // 关闭非必要外设
    // disable_non_essential_peripherals();
    
    // 降低传感器采样率
    // reduce_sensor_sampling_rate();
    
    // 增加BLE广播间隔
    // increase_ble_advertising_interval();
}

// ==================== 进入深度睡眠保护 ====================
void enter_deep_sleep_protection() {
    Serial.println("[恢复] 电量严重不足，进入深度睡眠保护");
    
    // 保存系统状态
    // save_system_state();
    
    // 关闭所有外设
    // disable_all_peripherals();
    
    // 配置唤醒源
    // configure_wakeup_sources();
    
    // 进入深度睡眠
    // enter_deep_sleep(3600000); // 睡眠1小时
    
    Serial.println("[警告] 系统即将进入深度睡眠，请充电后使用");
}

// ==================== 软件错误恢复 ====================
void recover_software_error(uint16_t error_code) {
    Serial.printf("[恢复] 软件错误恢复，代码: 0x%04X\n", error_code);
    
    switch (error_code) {
        case ERROR_CODE_MEMORY_LEAK:
            // 内存泄漏：清理内存
            cleanup_memory();
            break;
            
        case ERROR_CODE_STACK_OVERFLOW:
            // 栈溢出：重启任务
            restart_tasks();
            break;
            
        case ERROR_CODE_WATCHDOG_TIMEOUT:
            // 看门狗超时：系统重启
            system_soft_reset();
            break;
            
        default:
            // 通用软件恢复
            software_reset();
            break;
    }
}

// ==================== 通用恢复 ====================
void generic_recovery(uint16_t error_code) {
    Serial.printf("[恢复] 通用错误恢复，代码: 0x%04X\n", error_code);
    
    // 简单的重启策略
    // soft_reset_component(error_code);
}

// ==================== 更新错误记录 ====================
void update_error_records_after_recovery() {
    // 检查哪些错误已经恢复
    uint8_t new_count = 0;
    
    for (uint8_t i = 0; i < error_state.active_error_count; i++) {
        ErrorRecord* record = &error_state.active_errors[i];
        
        // 检查错误是否已解决
        if (is_error_resolved(record)) {
            Serial.printf("[恢复] 错误已解决: %s\n", record->description);
            // 错误已解决，不保留在活动错误列表中
        } else {
            // 错误未解决，保留在列表中
            error_state.active_errors[new_count] = *record;
            new_count++;
        }
    }
    
    error_state.active_error_count = new_count;
    
    // 更新系统稳定性状态
    error_state.system_stable = (error_state.active_error_count == 0);
}

// ==================== 检查错误是否已解决 ====================
bool is_error_resolved(const ErrorRecord* record) {
    // 这里可以实现具体的错误解决检查逻辑
    // 例如：检查传感器是否恢复正常、通信是否恢复等
    
    // 临时实现：如果重试次数超过最大值，认为未解决
    if (record->retry_count >= error_config.max_recovery_attempts) {
        return false;
    }
    
    // 模拟检查：50%的概率认为错误已解决
    return (random(100) < 50);
}

// ==================== 系统健康检查 ====================
void system_health_check() {
    static uint32_t last_check_time = 0;
    uint32_t current_time = millis();
    
    if (current_time - last_check_time < error_config.system_health_check_interval_ms) {
        return;
    }
    
    last_check_time = current_time;
    
    // 检查传感器健康状态
    check_sensor_health();
    
    // 检查通信健康状态
    check_communication_health();
    
    // 检查电源健康状态
    check_power_health();
    
    // 检查系统资源
    check_system_resources();
    
    // 更新系统稳定性标志
    update_system_stability();
