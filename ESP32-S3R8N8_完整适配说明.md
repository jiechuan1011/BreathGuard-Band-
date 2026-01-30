# ESP32-S3R8N8 完整适配说明

## 概述

本文档提供了糖尿病初筛腕带项目从ESP32-C3迁移到ESP32-S3R8N8的完整适配方案。所有修改已针对硬件更新、BLE通信、低功耗优化等要求进行了完整实现。

## 硬件更新

**原硬件**：ESP32-C3 SuperMini
**新硬件**：ESP32-S3R8N8（双核240MHz，8MB Flash + 8MB Octal PSRAM）

## 主要修改内容

### 1. I2C引脚配置修改
- **修改文件**：`config/pin_config.h`
- **修改内容**：
  ```cpp
  // 修改前（ESP32-C3）：
  #define PIN_SDA         4   // I2C数据线（ESP32-C3 GPIO4）
  #define PIN_SCL         5   // I2C时钟线（ESP32-C3 GPIO5）
  
  // 修改后（ESP32-S3）：
  #define PIN_SDA         4   // I2C数据线（ESP32-S3 GPIO4）
  #define PIN_SCL         5   // I2C时钟线（ESP32-S3 GPIO5）
  ```
- **说明**：ESP32-S3的GPIO4/5同样支持I2C，保持引脚兼容性

### 2. BLE配置新增
- **新增文件**：`config/ble_config.h`
- **内容**：
  ```cpp
  // MIT App Inventor APP使用的UUID
  #define BLE_SERVICE_UUID        "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
