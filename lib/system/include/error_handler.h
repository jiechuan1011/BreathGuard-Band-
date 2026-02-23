// Copied from system/error_handler.h for LDF
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 错误代码与配置结构（摘录）
#define ERROR_CODE_SENSOR_TIMEOUT          0x1001
// ... (保留原始头文件中完整定义以便编译)

typedef struct {
    uint8_t max_recovery_attempts;
    uint32_t recovery_cooldown_ms;
    uint32_t error_report_interval_ms;
    uint32_t sensor_timeout_ms;
    uint32_t ble_reconnect_interval_ms;
    uint8_t ble_max_reconnect_attempts;
    uint8_t low_battery_threshold;
    uint8_t critical_battery_threshold;
    uint32_t sensor_calibration_interval_ms;
    uint32_t system_health_check_interval_ms;
} ErrorHandlerConfig;

void error_handler_init();
void error_handler_record_error(uint8_t type, uint8_t severity, uint16_t error_code, const char* description);
void error_handler_clear_errors();
uint8_t error_handler_get_active_error_count();
bool error_handler_is_system_stable();
void error_handler_perform_health_check();
void error_handler_handle_sensor_error(uint16_t error_code, const void* sensor_data);
void error_handler_handle_communication_error(uint16_t error_code, const void* connection_info);
void error_handler_handle_power_error(uint16_t error_code, uint8_t battery_level);
void error_handler_get_statistics(uint32_t* total_errors, uint32_t* recovered_errors, uint32_t* critical_errors);
void error_handler_reset_statistics();
void error_handler_set_config(const ErrorHandlerConfig* config);
ErrorHandlerConfig error_handler_get_config();
bool error_handler_force_recovery(bool force_restart);
const char* error_handler_get_error_description(uint16_t error_code);
bool error_handler_is_error_recoverable(uint16_t error_code);
void error_handler_task();

#ifdef __cplusplus
}
#endif

#endif // ERROR_HANDLER_H
