// config/ble_config.h
// BLE配置头文件 - ESP32-S3 NimBLE配置
// ==============================================

#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include <Arduino.h>

// ==================== BLE UUID配置 ====================
// MIT App Inventor APP使用的UUID
#define BLE_SERVICE_UUID        "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
// ==============================================

#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include <Arduino.h>

// ==================== BLE UUID配置 ====================
// 服务UUID（与APP文档一致）
#define BLE_SERVICE_UUID        "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
#define BLE_CHARACTERISTIC_UUID "a1b2c3d4-e5f6-4789-abcd-ef012345678a"

// ==================== 设备配置 ====================
#define BLE_DEVICE_NAME         "DiabetesSensor"  // 设备广播名称
#define BLE_ADVERTISING_INTERVAL_MS 500           // 广播间隔（毫秒）
#define BLE_NOTIFY_INTERVAL_MS  4000              // 数据通知间隔（毫秒）

// ==================== 数据格式配置 ====================
// JSON数据最大长度
#define BLE_JSON_BUFFER_SIZE    128

// 丙酮无效值（腕带无此数据）
#define ACETONE_INVALID_VALUE   -99

// ==================== 功耗优化配置 ====================
// 使用NimBLE库（更省电）
#ifdef USE_NIMBLE
    #include <NimBLEDevice.h>
    #include <NimBLEUtils.h>
    #include <NimBLEServer.h>
    #include <NimBLEAdvertising.h>
    #include <NimBLECharacteristic.h>
    
    // NimBLE类型别名
    #define BLE_SERVER_TYPE NimBLEServer*
    #define BLE_SERVICE_TYPE NimBLEService*
    #define BLE_CHARACTERISTIC_TYPE NimBLECharacteristic*
    #define BLE_ADVERTISING_TYPE NimBLEAdvertising*
#else
    // 使用默认ESP32 BLE库
    #include <BLEDevice.h>
    #include <BLEUtils.h>
    #include <BLEServer.h>
    #include <BLEAdvertising.h>
    #include <BLECharacteristic.h>
    
    // 默认BLE类型别名
    #define BLE_SERVER_TYPE BLEServer*
    #define BLE_SERVICE_TYPE BLEService*
    #define BLE_CHARACTERISTIC_TYPE BLECharacteristic*
    #define BLE_ADVERTISING_TYPE BLEAdvertising*
#endif

// ==================== 全局变量声明 ====================
// 这些变量在main_control.ino中定义
extern BLE_SERVER_TYPE pServer;
extern BLE_SERVICE_TYPE pService;
extern BLE_CHARACTERISTIC_TYPE pCharacteristic;
extern bool deviceConnected;
extern bool oldDeviceConnected;

// ==================== 函数声明 ====================
// BLE初始化函数
void ble_init();
void ble_start_advertising();

// 数据打包和发送函数
String ble_pack_json_data(uint8_t hr_bpm, uint8_t spo2, float acetone, const char* note);
void ble_send_data(const String& json_data);

// 连接状态回调函数
void ble_on_connect();
void ble_on_disconnect();

#endif // BLE_CONFIG_H