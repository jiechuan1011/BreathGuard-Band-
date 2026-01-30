// config/ble_config.h
// BLE配置头文件 - ESP32-S3 NimBLE配置
// ==============================================

#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include <Arduino.h>

// ==================== BLE UUID配置 ====================
// MIT App Inventor APP使用的UUID
#define BLE_SERVICE_UUID        "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
#define BLE_CHARACTERISTIC_UUID "a1b2c3d4-e5f6-4789-abcd-ef012345678a"

// 设备名称
#define BLE_DEVICE_NAME         "DiabetesSensor"

// ==================== BLE参数配置 ====================
// 广播间隔（单位：0.625ms）
#define BLE_ADV_INTERVAL_MIN    800   // 500ms (800 * 0.625ms)
#define BLE_ADV_INTERVAL_MAX    1600  // 1000ms (1600 * 0.625ms)

// 连接参数
#define BLE_CONN_INTERVAL_MIN   6     // 7.5ms (6 * 1.25ms)
#define BLE_CONN_INTERVAL_MAX   12    // 15ms (12 * 1.25ms)
#define BLE_CONN_LATENCY        0     // 无延迟
#define BLE_CONN_TIMEOUT        400   // 4秒 (400 * 10ms)

// 特征值属性
#define BLE_CHAR_PROPERTIES     (NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY)

// ==================== 数据格式配置 ====================
// JSON数据最大长度
#define BLE_JSON_MAX_LENGTH     128

// 数据发送间隔（毫秒）
#define BLE_NOTIFY_INTERVAL_MS  4000  // 4秒

// ==================== 低功耗配置 ====================
// ESP32-S3功耗等级（ESP_PWR_LVL_P3为最低功耗）
#define BLE_POWER_LEVEL         ESP_PWR_LVL_P3

// 连接时是否启用低功耗模式
#define BLE_LOW_POWER_MODE      1

#endif // BLE_CONFIG_H