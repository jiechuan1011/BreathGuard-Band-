#include "hr_driver.h"
#include <Wire.h>
#include <MAX30105.h>  // SparkFun MAX3010x 库的主头文件（MAX30105）

// ──────────────────────────────────────────────
// 使用SparkFun_MAX3010x库的MAX30102驱动

static MAX30105 max30102;  // 使用MAX30105类，兼容MAX30102
static bool sensor_initialized = false;

bool hr_driver_init() {
    Wire.begin();
    
    // 初始化MAX30102传感器
    if (!max30102.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("[HR] MAX30102初始化失败，请检查连接");
        return false;
    }
    
    // 配置传感器参数
    max30102.setup();  // 使用默认配置
    
    // 设置采样率100Hz
    max30102.setSampleRate(HR_SAMPLE_RATE);
    
    // 设置脉宽411us（推荐值）
    max30102.setPulseWidth(HR_PULSE_WIDTH);
    
    // 设置LED电流（0x0A = 约10mA）
    max30102.setPulseAmplitudeRed(HR_LED_CURRENT);  // 红光LED电流
    max30102.setPulseAmplitudeIR(HR_LED_CURRENT);   // 红外LED电流
    
    // 启用SpO2模式（SparkFun 库不提供 setMode，使用默认配置或按需改为 setLEDMode）
    // 如果需要显式设置，请替换为：max30102.setLEDMode(<mode>);
    
    // 清除FIFO
    max30102.clearFIFO();
    
    sensor_initialized = true;
    Serial.println("[HR] MAX30102初始化成功（使用SparkFun库）");
    return true;
}

bool hr_available() {
    if (!sensor_initialized) return false;
    return max30102.available();  // 检查是否有新数据
}

bool hr_read_latest(int32_t* red, int32_t* ir) {
    if (!sensor_initialized) return false;
    
    // 确保有数据可读
    if (!max30102.available()) {
        return false;
    }
    
    // 读取最新数据
    *red = (int32_t)max30102.getRed();
    *ir = (int32_t)max30102.getIR();
    
    // 准备读取下一个样本
    max30102.nextSample();
    
    return true;
}

void hr_shutdown() {
    if (sensor_initialized) {
        max30102.shutDown();
    }
}

void hr_wakeup() {
    if (sensor_initialized) {
        max30102.wakeUp();
    }
}

float hr_read_temperature() {
    if (!sensor_initialized) return NAN;
    return max30102.readTemperature();
}
