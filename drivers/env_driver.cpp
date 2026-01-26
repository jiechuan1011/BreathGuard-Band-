#include "env_driver.h"
#include <Wire.h>
#include <math.h>

#if ENV_USE_MOCK

// ================= MOCK MODE =================
// 模拟温湿度数据（调试用）

bool env_init() {
    return true;
}

bool env_read(EnvData* out) {
    if (!out) return false;

    // 模拟平滑变化
    float t = millis() * 0.001f;
    out->temperature_c = 25.0f + 5.0f * sin(2 * PI * t / 60.0f);  // 20-30℃
    out->humidity_rh = 50.0f + 20.0f * sin(2 * PI * t / 45.0f);   // 30-70%
    out->valid = true;

    delay(20);  // 模拟I2C延迟
    return true;
}

#else

#if ENV_USE_I2C

// ================= I2C MODE (DHT20/AHT20) =================

static bool i2c_write(uint8_t reg, uint8_t* data, uint8_t len) {
    Wire.beginTransmission(ENV_I2C_ADDR);
    Wire.write(reg);
    if (data && len > 0) {
        Wire.write(data, len);
    }
    return (Wire.endTransmission() == 0);
}

static bool i2c_read(uint8_t* buf, uint8_t len) {
    Wire.requestFrom(ENV_I2C_ADDR, len);
    if (Wire.available() < len) return false;
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    return true;
}

bool env_init() {
    Wire.begin();
    delay(100);  // 等待传感器稳定

    // DHT20/AHT20 初始化：发送测量命令
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};
    if (!i2c_write(0x00, cmd, 3)) {
        return false;
    }
    delay(80);  // 等待测量完成
    return true;
}

bool env_read(EnvData* out) {
    if (!out) return false;

    // 发送测量命令
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};
    if (!i2c_write(0x00, cmd, 3)) {
        out->valid = false;
        return false;
    }

    delay(80);  // 等待测量完成（DHT20/AHT20 典型80ms）

    // 读取6字节数据
    uint8_t data[6];
    if (!i2c_read(data, 6)) {
        out->valid = false;
        return false;
    }

    // 检查忙标志（第1字节最高位）
    if (data[0] & 0x80) {
        out->valid = false;
        return false;  // 仍在测量中
    }

    // 解析温湿度（DHT20/AHT20 格式）
    uint32_t raw_rh = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t raw_temp = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    out->humidity_rh = (raw_rh * 100.0f) / 1048576.0f;  // 2^20
    out->temperature_c = (raw_temp * 200.0f) / 1048576.0f - 50.0f;  // -50~150℃

    // 合理性检查
    if (out->humidity_rh < 0 || out->humidity_rh > 100 ||
        out->temperature_c < -40 || out->temperature_c > 85) {
        out->valid = false;
        return false;
    }

    out->valid = true;
    return true;
}

#else

// ================= ANALOG MODE =================
// 占位实现：假设模拟传感器输出线性映射

bool env_init() {
    pinMode(ENV_ANALOG_TEMP_PIN, INPUT);
    pinMode(ENV_ANALOG_RH_PIN, INPUT);
    return true;
}

bool env_read(EnvData* out) {
    if (!out) return false;

    // 读取ADC并线性映射（需根据实际传感器校准）
    int temp_adc = analogRead(ENV_ANALOG_TEMP_PIN);
    int rh_adc = analogRead(ENV_ANALOG_RH_PIN);

    // 简单线性映射（示例：0-1023 -> 0-100℃ / 0-100%RH）
    out->temperature_c = (temp_adc / 1023.0f) * 100.0f;
    out->humidity_rh = (rh_adc / 1023.0f) * 100.0f;

    // 合理性检查
    if (out->humidity_rh < 0 || out->humidity_rh > 100 ||
        out->temperature_c < -40 || out->temperature_c > 85) {
        out->valid = false;
        return false;
    }

    out->valid = true;
    return true;
}

#endif  // #if ENV_USE_I2C

#endif  // #if ENV_USE_MOCK
