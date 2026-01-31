/*
 * error_handler.h - 错误处理增强模块头文件
 * 
 * 功能：传感器故障恢复机制、BLE断线重连、电池低压保护
 * 设计原则：
 * 1. 分层错误处理
 * 2. 自动恢复机制
 * 3. 错误日志记录
 * 4. 用户友好提示
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 错误代码定义 ====================
// 传感器错误代码
#define ERROR_CODE_SENSOR_TIMEOUT          0x1001
#define ERROR_CODE_SENSOR_CALIBRATION      0x1002
#define ERROR_CODE_SENSOR_COMMUNICATION    0x1003
#define ERROR_CODE_SENSOR_OUT_OF_RANGE     0x1004

// 通信错误代码
#define ERROR_CODE_BLE_DISCONNECTED        0x2001
#define ERROR_CODE_BLE_CONNECTION_FAILED   0x2002
#define ERROR_CODE_I2C_ERROR               0x2003
#define ERROR_CODE_SPI_ERROR               0x2004

// 电源错误代码
#define ERROR_CODE_LOW_BATTERY             0x3001
#define ERROR_CODE_BATTERY_CRITICAL        0x3002
#define ERROR_CODE_POWER_FLUCTUATION       0x3003
#define ERROR_CODE_CHARGING_FAULT          0x3004

// 硬件错误代码
#define ERROR_CODE_MEMORY_CORRUPTION       0x4001
#define ERROR_CODE_FLASH_WRITE_FAILED      0x4002
#define ERROR_CODE_HARDWARE_FAULT          0x4003

// 软件错误代码
#define ERROR_CODE_MEMORY_LEAK             0x5001
#define ERROR_CODE_STACK_OVERFLOW          0x5002
#define ERROR_CODE_WATCHDOG_TIMEOUT        0x5003
#define ERROR_CODE_SOFTWARE_EXCEPTION      0x5004

// 用户错误代码
#define ERROR_CODE_USER_INPUT_INVALID      0x6001
#define ERROR_CODE_CONFIGURATION_ERROR     0x6002

// ==================== 错误处理配置 ====================
#define MAX_ACTIVE_ERRORS      10  // 最大活动错误数
#define MAX_ERROR_HISTORY      50  // 最大错误历史记录数

typedef struct {
    uint8_t max_recovery_attempts;          // 最大恢复尝试次数
    uint32_t recovery_cooldown_ms;          // 恢复冷却时间（毫秒）
    uint32_t error_report_interval_ms;      // 错误报告间隔（毫秒）
    uint32_t sensor_timeout_ms;             // 传感器超时时间（毫秒）
    uint32_t ble_reconnect_interval_ms;     // BLE重连间隔（毫秒）
    uint8_t ble_max_reconnect_attempts;     // BLE最大重连尝试次数
    uint8_t low_battery_threshold;          // 低电量阈值（%）
    uint8_t critical_battery_threshold;     // 严重电量阈值（%）
    uint32_t sensor_calibration_interval_ms; // 传感器校准间隔（毫秒）
    uint32_t system_health_check_interval_ms; // 系统健康检查间隔（毫秒）
} ErrorHandlerConfig;

// ==================== 公共接口函数 ====================

/**
 * @brief 初始化错误处理模块
 */
void error_handler_init();

/**
 * @brief 记录错误
 * @param type 错误类型
 * @param severity 错误严重程度
 * @param error_code 错误代码
 * @param description 错误描述
 */
void error_handler_record_error(uint8_t type, uint8_t severity, 
                                uint16_t error_code, const char* description);

/**
 * @brief 清除所有活动错误
 */
void error_handler_clear_errors();

/**
 * @brief 获取活动错误数量
 * @return 活动错误数量
 */
uint8_t error_handler_get_active_error_count();

/**
 * @brief 检查系统是否稳定
 * @return true: 系统稳定, false: 系统不稳定
 */
bool error_handler_is_system_stable();

/**
 * @brief 执行系统健康检查
 */
void error_handler_perform_health_check();

/**
 * @brief 处理传感器错误
 * @param error_code 错误代码
 * @param sensor_data 传感器数据（可选）
 */
void error_handler_handle_sensor_error(uint16_t error_code, const void* sensor_data);

/**
 * @brief 处理通信错误
 * @param error_code 错误代码
 * @param connection_info 连接信息（可选）
 */
void error_handler_handle_communication_error(uint16_t error_code, const void* connection_info);

/**
 * @brief 处理电源错误
 * @param error_code 错误代码
 * @param battery_level 电池电量（%）
 */
void error_handler_handle_power_error(uint16_t error_code, uint8_t battery_level);

/**
 * @brief 获取错误统计信息
 * @param total_errors 总错误数
 * @param recovered_errors 已恢复错误数
 * @param critical_errors 严重错误数
 */
void error_handler_get_statistics(uint32_t* total_errors, 
                                  uint32_t* recovered_errors,
                                  uint32_t* critical_errors);

/**
 * @brief 重置错误统计
 */
void error_handler_reset_statistics();

/**
 * @brief 设置错误处理配置
 * @param config 新的配置
 */
void error_handler_set_config(const ErrorHandlerConfig* config);

/**
 * @brief 获取当前错误处理配置
 * @return 当前配置
 */
ErrorHandlerConfig error_handler_get_config();

/**
 * @brief 强制系统恢复
 * @param force_restart 是否强制重启
 * @return true: 恢复成功, false: 恢复失败
 */
bool error_handler_force_recovery(bool force_restart);

/**
 * @brief 获取错误描述
 * @param error_code 错误代码
 * @return 错误描述字符串
 */
const char* error_handler_get_error_description(uint16_t error_code);

/**
 * @brief 检查错误是否可恢复
 * @param error_code 错误代码
 * @return true: 可恢复, false: 不可恢复
 */
bool error_handler_is_error_recoverable(uint16_t error_code);

/**
 * @brief 主错误处理任务（在主循环中调用）
 */
void error_handler_task();

#ifdef __cplusplus
}
#endif

#endif // ERROR_HANDLER_H