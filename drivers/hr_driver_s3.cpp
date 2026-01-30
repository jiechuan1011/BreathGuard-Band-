// drivers/hr_driver_s3.cpp
// ESP32-S3R8N8专用的MAX30102驱动（使用SparkFun MAX30105库）
// ==============================================

#include "hr_driver.h"
#include <Wire.h>
#include <SparkFun_MAX30105.h>

// MAX30102传感器对象
MAX30105 particleSensor;

// 采样缓冲区
static int32_t last_red = 0;
static int32_t last_ir = 0;
static bool sensor_initialized = false;

// I2C写函数
static bool i2c_write(uint8_t reg, uint8_t val) {
    for (uint8_t retry = 0; retry < HR_I2C_RETRY_TIMES; retry++) {
        Wire.beginTransmission(MAX30102_I2C_ADDR);
        Wire.write(reg);
        Wire.write(val);
        if (Wire.endTransmission() == 0) {
            return true;
        }
        delay(2);
    }
    return false;
}

// I2C读函数
static bool i2c_read(uint8_t reg, uint8_t* buf, uint8_t len) {
    for (uint8_t retry = 0; retry < HR_I2C_RETRY_TIMES; retry++) {
        Wire.beginTransmission(MAX30102_I2C_ADDR);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) {
            delay(2);
            continue;
        }

        Wire.requestFrom(MAX30102_I2C_ADDR, len);
        if (Wire.available() >= len) {
            for (uint8_t i = 0; i < len; i++) {
                buf[i] = Wire.read();
            }
            return true;
        }
        delay(2);
    }
    return false;
}

// 使用SparkFun库初始化MAX30102
bool hr_init() {
    Serial.println("[HR Driver] 初始化MAX30102 (ESP32-S3)...");
    
    // 初始化I2C
    Wire.begin(PIN_SDA, PIN_SCL);
    
    // 使用SparkFun库初始化
    if (!particleSensor.begin(Wire, MAX30102_I2C_ADDR)) {
        Serial.println("[HR Driver] MAX30102初始化失败");
        return false;
    }
    
    // 配置传感器参数
    byte ledBrightness = 60;  // 0-255
    byte sampleAverage = 4;   // 1, 2, 4, 8, 16, 32
    byte ledMode = 2;         // 2 = Red + IR
    int sampleRate = 100;     // 100Hz
    int pulseWidth = 411;     // 411us
    int adcRange = 4096;      // 4096nA
    
    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    
    // 启用温度传感器（可选）
    particleSensor.enableDIETEMPRDY();
    
    sensor_initialized = true;
    Serial.println("[HR Driver] MAX30102初始化成功");
    return true;
}

bool hr_available() {
    if (!sensor_initialized) return false;
    return particleSensor.available();
}

bool hr_read_sample(int32_t* red, int32_t* ir) {
    if (!sensor_initialized) return false;
    
    if (particleSensor.available()) {
        *red = particleSensor.getRed();
        *ir = particleSensor.getIR();
        particleSensor.nextSample();
        
        // 存储最新数据
        last_red = *red;
        last_ir = *ir;
        
        return true;
    }
    
    return false;
}

bool hr_read_latest(int32_t* red, int32_t* ir) {
    if (!sensor_initialized) return false;
    
    // 尝试读取新样本
    if (hr_read_sample(red, ir)) {
        return true;
    }
    
    // 如果没有新样本，返回上次读取的数据
    if (last_red != 0 || last_ir != 0) {
        *red = last_red;
        *ir = last_ir;
        return true;
    }
    
    return false;
}

void hr_shutdown() {
    if (sensor_initialized) {
        // 使用SparkFun库的shutdown函数
        particleSensor.shutDown();
        sensor_initialized = false;
        Serial.println("[HR Driver] MAX30102已关闭");
    }
}

void hr_wakeup() {
    if (!sensor_initialized) {
        hr_init();
    } else {
        // 使用SparkFun库的wakeUp函数
        particleSensor.wakeUp();
        Serial.println("[HR Driver] MAX30102已唤醒");
    }
}

float hr_read_temperature() {
    if (!sensor_initialized) return NAN;
    
    // 使用SparkFun库读取温度
    return particleSensor.readTemperature();
}