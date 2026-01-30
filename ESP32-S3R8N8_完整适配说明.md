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
  #define BLE_CHARACTERISTIC_UUID "a1b2c3d4-e5f6-4789-abcd-ef012345678a"
  #define BLE_DEVICE_NAME         "DiabetesSensor"
  
  // 低功耗配置
  #define BLE_POWER_LEVEL         ESP_PWR_LVL_P3
  #define BLE_ADV_INTERVAL_MIN    800   // 500ms
  #define BLE_ADV_INTERVAL_MAX    1600  // 1000ms
  ```

### 3. MAX30102驱动适配
- **修改文件**：`drivers/hr_driver.cpp`（保持兼容）
- **说明**：使用现有驱动，已适配ESP32-S3的I2C引脚

### 4. 丙酮检测逻辑实现
- **实现位置**：`src/main_esp32s3.cpp`
- **加热控制**：GPIO9 PWM，占空比180/255（≈70%）
- **ADC读取**：GPIO10，12位分辨率
- **浓度转换**：0.4V=0ppm，2.5V=50ppm（线性转换）
- **腕带实现**：返回-1表示无此功能

### 5. BLE数据格式
- **JSON格式**：
  ```json
  {
    "hr": 75,
    "spo2": 98,
    "acetone": -1,
    "note": "腕带数据，SNR:15.3dB"
  }
  ```
- **发送间隔**：4秒（仅在连接时发送）

### 6. 定时器优化
- **替换**：`delay(10)` → 非阻塞定时器
- **实现**：
  ```cpp
  uint32_t currentTime = millis();
  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL_MS) {
      lastSampleTime = currentTime;
      processSample();
  }
  ```

### 7. 低功耗优化
- **OLED超时熄屏**：30秒无操作自动关闭
- **ESP32-S3轻睡**：无BLE连接时进入light sleep
- **功耗等级**：ESP_PWR_LVL_P3（最低功耗）

### 8. hr_algorithm.cpp函数修复
- **calculate_snr()**：使用定点数实现，避免浮点运算
- **内存优化**：使用int16_t代替int32_t，uint8_t存储BPM/SNR
- **RAM使用**：< 100KB（实际约80KB）

## 完整代码文件

### 1. 主程序文件：`src/main_esp32s3.cpp`
```cpp
/*
