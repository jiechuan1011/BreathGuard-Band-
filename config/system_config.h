// config/system_config.h
// 系统参数配置文件
// ==============================================
// 使用说明：
// 1. 包含所有系统级配置参数
// 2. 参数分为通用配置和角色专用配置
// 3. 所有时间单位均为毫秒(ms)，除非特别说明
// ==============================================

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "config.h"

// ==================== 通用系统配置 ====================

// 系统时钟配置
#define SYSTEM_TICK_MS          10      // 系统心跳周期（10ms）
#define WATCHDOG_TIMEOUT_MS     5000    // 看门狗超时时间（5秒）

// 串口通信配置
#define SERIAL_BAUDRATE         115200  // 串口波特率
#define SERIAL_TIMEOUT_MS       1000    // 串口超时时间

// 电源管理配置
#define BATTERY_CHECK_INTERVAL_MS 60000  // 电池检查间隔（60秒）
#define LOW_BATTERY_THRESHOLD   3.3     // 低电量阈值（V）
#define CRITICAL_BATTERY_THRESHOLD 3.0  // 临界电量阈值（V）

// 数据存储配置
#define DATA_BUFFER_SIZE        1024    // 数据缓冲区大小（字节）
#define MAX_HISTORY_RECORDS     100     // 最大历史记录数

// ==================== 腕带主控专用配置 ====================
#ifdef DEVICE_ROLE_WRIST

// 心率血氧采集配置
#define HR_SAMPLE_RATE          100     // 心率采样率（Hz）
#define HR_BUFFER_SIZE          1000    // 心率缓冲区大小（10秒数据）
#define HR_ALGORITHM_WINDOW_MS  10000   // 心率算法窗口（10秒）
#define HR_MIN_BPM              40      // 最小有效心率
#define HR_MAX_BPM              180     // 最大有效心率
#define HR_SNR_THRESHOLD_DB     10      // 信噪比阈值（dB）

// OLED显示配置
#define OLED_REFRESH_INTERVAL_MS 1000   // OLED刷新间隔（1秒）
#define OLED_TIMEOUT_MS         30000   // OLED自动关闭时间（30秒）

// BLE通信配置
#define BLE_ADVERTISE_INTERVAL_MS 100   // BLE广播间隔
#define BLE_CONNECT_TIMEOUT_MS   10000  // BLE连接超时
#define BLE_MTU_SIZE             23     // BLE MTU大小

// 运动校正配置
#define MOTION_SAMPLE_RATE       50     // 运动传感器采样率（Hz）
#define MOTION_THRESHOLD_G       0.2    // 运动检测阈值（g）

#endif // DEVICE_ROLE_WRIST

// ==================== 检测模块专用配置 ====================
#ifdef DEVICE_ROLE_DETECTOR

// 气体传感器配置
#define GAS_SAMPLE_RATE          10     // 气体采样率（Hz）
#define GAS_BUFFER_SIZE          100    // 气体缓冲区大小（10秒数据）
#define GAS_BASELINE_SAMPLES     50     // 基线采样数
#define GAS_CALIBRATION_TIME_MS  30000  // 校准时间（30秒）
#define GAS_HEATING_TEMP_C       350    // 目标加热温度（℃）
#define GAS_WARMUP_TIME_MS       60000  // 预热时间（60秒）

// 加热控制配置
#define HEATER_PWM_FREQUENCY     1000   // PWM频率（Hz）
#define HEATER_PWM_RESOLUTION    8      // PWM分辨率（位）
#define HEATER_MAX_CURRENT_MA    500    // 最大加热电流（mA）
#define HEATER_OVER_TEMP_C       400    // 过热保护温度（℃）

// 呼吸传感器配置
#define RESP_SAMPLE_RATE         100    // 呼吸采样率（Hz）
#define RESP_BUFFER_SIZE         500    // 呼吸缓冲区大小（5秒数据）
#define RESP_MIN_RATE            6      // 最小呼吸率（次/分钟）
#define RESP_MAX_RATE            30     // 最大呼吸率（次/分钟）
#define RESP_DETECTION_THRESHOLD 0.5    // 呼吸检测阈值（V）

// AD623运放配置
#define AD623_GAIN              100     // 运放增益（倍）
#define AD623_REF_VOLTAGE       1.65    // 参考电压（V）
#define AD623_MAX_OUTPUT_V      3.0     // 最大输出电压（V）

// RC滤波配置
#define RC_FILTER_CUTOFF_HZ     10      // 截止频率（Hz）
#define RC_FILTER_TAU_MS        15.9    // 时间常数τ=1/(2πfc)（ms）

#endif // DEVICE_ROLE_DETECTOR

// ==================== 算法参数配置 ====================

// 数据滤波参数
#define MOVING_AVERAGE_WINDOW   10      // 移动平均窗口大小
#define MEDIAN_FILTER_WINDOW    5       // 中值滤波窗口大小
#define LOW_PASS_CUTOFF_HZ      5       // 低通滤波器截止频率

// 信号质量评估
#define MIN_SIGNAL_QUALITY      0.5     // 最小信号质量（0-1）
#define MAX_NOISE_RATIO         0.3     // 最大噪声比

// 风险评估参数（仅趋势分析，非诊断）
#define TREND_WINDOW_SIZE       10      // 趋势分析窗口大小
#define TREND_THRESHOLD_PERCENT 20      // 趋势变化阈值（%）

// ==================== 安全限制配置 ====================

// 温度安全限制
#define MAX_OPERATING_TEMP_C    60      // 最高工作温度（℃）
#define MIN_OPERATING_TEMP_C    0       // 最低工作温度（℃）

// 电气安全限制
#define MAX_CURRENT_MA          1000    // 最大工作电流（mA）
#define MAX_VOLTAGE_V           5.0     // 最大工作电压（V）

// 时间安全限制
#define MAX_CONTINUOUS_OPERATION_MS 3600000  // 最大连续运行时间（1小时）
#define MIN_REST_TIME_MS        60000   // 最小休息时间（1分钟）

// ==================== 调试与测试配置 ====================

#ifdef DEBUG_MODE
// 调试模式下的特殊配置
#define TEST_MODE_ENABLED       true    // 启用测试模式
#define VERBOSE_LOGGING         true    // 启用详细日志
#define SENSOR_SIMULATION       false   // 传感器模拟（测试用）
#else
// 生产模式配置
#define TEST_MODE_ENABLED       false   // 禁用测试模式
#define VERBOSE_LOGGING         false   // 禁用详细日志
#define SENSOR_SIMULATION       false   // 禁用传感器模拟
#endif // DEBUG_MODE

#endif // SYSTEM_CONFIG_H
