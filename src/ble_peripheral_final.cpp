/*
 * ble_peripheral_final.cpp - BLE主从通信（最终版）
 * 
 * 数据格式（用户指定）：
 * {
 *   "hr": 85,
 *   "spo2": 97,
 *   "acetone": 12.4,
 *   "battery": 92,
 *   "snr": 23,
 *   "timestamp": 1740123456,
 *   "risk_level": "中风险"
 * }
 * 
 * 使用NimBLE库实现ESP32-S3 BLE Server
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLE2902.h>
#include <ArduinoJson.h>
#include "ble_peripheral_final.h"
#include "algorithm_manager_final.h"
#include "sensor_collector_final.h"

// ==================== BLE GATT定义 ====================

#define SERVICE_UUID        "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
#define CHAR_UUID           "a1b2c3d4-e5f6-4789-abcd-ef012345678a"

// ==================== 全局BLE状态 ====================

typedef struct {
    NimBLEServer* server;
    NimBLEService* service;
    NimBLECharacteristic* characteristic;
    
    uint8_t is_connected;
    uint32_t last_notify_ms;
    uint32_t notify_interval_ms;
    
    uint32_t total_notifications;
    uint32_t total_bytes_sent;
    
    // 风险评估数据
    uint8_t risk_level;
    char risk_desc[32];
} BlePeripheralState;

static BlePeripheralState g_ble = {0};

// ==================== BLE回调 ====================

class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        g_ble.is_connected = 1;
        Serial.println("[BLE] 连接成功");
        
        // 更新连接参数（降低延迟）
        pServer->updateConnParams(desc->conn_handle, 40, 80, 0, 400);
    }
    
    void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        g_ble.is_connected = 0;
        Serial.println("[BLE] 连接断开，重启广播");
        
        // 重启广播
        NimBLEAdvertising* pAdvertising = pServer->getAdvertising();
        pAdvertising->start();
    }
};

// ==================== 初始化 ====================

void ble_peripheral_init() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    
    // 创建Server
    g_ble.server = NimBLEDevice::createServer();
    g_ble.server->setCallbacks(new MyServerCallbacks());
    
    // 创建Service
    g_ble.service = g_ble.server->createService(SERVICE_UUID);
    
    // 创建Characteristic（可读+通知）
    g_ble.characteristic = g_ble.service->createCharacteristic(
        CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    // 添加CCCD（Client Characteristic Configuration Descriptor）
    g_ble.characteristic->addDescriptor(new NimBLE2902());
    
    // 启动Service
    g_ble.service->start();
    
    // 配置广播
    NimBLEAdvertising* pAdvertising = g_ble.server->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->start();
    
    g_ble.last_notify_ms = millis();
    g_ble.notify_interval_ms = 4000;  // 4秒通知一次
    g_ble.is_connected = 0;
    
    Serial.println("[BLE] Peripheral初始化完成");
    Serial.printf("    Service UUID: %s\n", SERVICE_UUID);
    Serial.printf("    Device Name: %s\n", BLE_DEVICE_NAME);
}

// ==================== 数据发送（JSON格式） ====================

void ble_peripheral_send_data() {
    if (!g_ble.is_connected) {
        return;  // 未连接
    }
    
    uint32_t now_ms = millis();
    if ((now_ms - g_ble.last_notify_ms) < g_ble.notify_interval_ms) {
        return;  // 未到通知时间
    }
    
    // 获取算法结果
    AlgorithmResult alg_result = {0};
    algorithm_manager_get_result(&alg_result);
    
    // 获取电池数据
    CollectorStats collector_stats = {0};
    sensor_collector_get_stats(&collector_stats);
    
    // 获取风险评估
    RiskAssessment risk = {0};
    algorithm_manager_get_risk_assessment(&risk);
    
    // 构建JSON（163字节左右，BLE MTU 23足够）
    StaticJsonDocument<256> doc;
    doc["hr"] = alg_result.bpm;
    doc["spo2"] = alg_result.spo2;
    doc["acetone"] = alg_result.acetone_ppm;
    doc["battery"] = collector_stats.battery_percent;
    doc["snr"] = alg_result.signal_quality;
    doc["timestamp"] = now_ms / 1000;  // Unix时间戳（秒）
    doc["risk_level"] = risk.risk_description;
    
    // 序列化为字符串
    String json_str;
    serializeJson(doc, json_str);
    
    // 分片发送（如果超过MTU size）
    const uint8_t* json_bytes = (const uint8_t*)json_str.c_str();
    uint16_t json_len = json_str.length();
    const uint16_t MAX_CHUNK = 20;  // BLE MTU 23 - overhead
    
    uint16_t offset = 0;
    while (offset < json_len) {
        uint16_t chunk_len = (json_len - offset > MAX_CHUNK) ? MAX_CHUNK : (json_len - offset);
        
        g_ble.characteristic->setValue(json_bytes + offset, chunk_len);
        g_ble.characteristic->notify();
        
        offset += chunk_len;
        delayMicroseconds(100);  // 避免过快发送
    }
    
    g_ble.last_notify_ms = now_ms;
    g_ble.total_notifications++;
    g_ble.total_bytes_sent += json_len;
    
#ifdef DEBUG_MODE
    if (g_ble.total_notifications % 3 == 0) {
        Serial.printf("[BLE] SEND #%lu: %s\n", g_ble.total_notifications, json_str.c_str());
    }
#endif
}

// ==================== 查询函数 ====================

uint8_t ble_peripheral_is_connected() {
    return g_ble.is_connected;
}

uint32_t ble_peripheral_get_notifications_sent() {
    return g_ble.total_notifications;
}

void ble_peripheral_get_stats(BleStats* stats) {
    if (stats == NULL) return;
    
    stats->is_connected = g_ble.is_connected;
    stats->total_notifications = g_ble.total_notifications;
    stats->total_bytes_sent = g_ble.total_bytes_sent;
}

void ble_peripheral_print_stats() {
#ifdef DEBUG_MODE
    Serial.printf("\n[BLE STATS] 连接:%s 通知:%lu 发送:%lu字节\n",
        g_ble.is_connected ? "✓" : "✗",
        g_ble.total_notifications,
        g_ble.total_bytes_sent);
#endif
}