#include "hr_driver.h"
#include <Wire.h>

#if HR_USE_MOCK

// ──────────────────────────────────────────────
// Mock 模式：生成模拟 PPG 信号（正弦波 + 轻微噪声）
static uint32_t mock_counter = 0;

bool hr_init() {
    mock_counter = 0;
    return true;
}

bool hr_available() {
    return true;  // mock 随时有数据
}

bool hr_read_sample(int32_t* red, int32_t* ir) {
    mock_counter++;
    float t = mock_counter * 0.01;  // 模拟 100Hz

    // 模拟心率 ≈ 72 bpm 的正弦波 + 轻微随机噪声
    float base = 48000 + 3000 * sin(2 * PI * 1.2 * t);   // 基线 + 心跳
    float noise = random(-80, 81);                       // ±80 噪声

    *ir  = (int32_t)(base + 200 * sin(2 * PI * 1.2 * t + 0.3) + noise);
    *red = (int32_t)(base * 0.96 + 180 * sin(2 * PI * 1.2 * t + 0.4) + noise * 0.9);

    delay(8);   // 模拟采集延迟
    return true;
}

bool hr_read_latest(int32_t* red, int32_t* ir) {
    return hr_read_sample(red, ir);
}

void hr_shutdown() { /* mock 无需实现 */ }
void hr_wakeup()   { /* mock 无需实现 */ }

float hr_read_temperature() {
    return 25.0f + (random(0, 21) - 10) * 0.1f;  // 模拟 20~30℃
}

#else

// ──────────────────────────────────────────────
// 真实 MAX30102 驱动（使用SparkFun_MAX3010x库兼容接口）

#include <SparkFun_MAX3010x.h>  // 使用SparkFun库简化驱动

static MAX30105 max30102;  // 使用MAX30105类，兼容MAX30102
static bool sensor_initialized = false;

bool hr_init() {
    Wire.begin();
    
    // 初始化MAX30102传感器
    if (!max30102.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("[HR] MAX30102初始化失败，请检查连接");
        return false;
    }
    
    // 配置传感器参数
    max30102.setup();  // 使用默认配置
    
    // 设置采样率100Hz
    max30102.setSampleRate(100);
    
    // 设置脉宽411us（推荐值）
    max30102.setPulseWidth(411);
    
    // 设置LED电流（适中值）
    max30102.setPulseAmplitudeRed(0x24);  // 红光LED电流
    max30102.setPulseAmplitudeIR(0x24);   // 红外LED电流
    
    // 启用SpO2模式
    max30102.setMode(MAX30105_MODE_SPO2);
    
    // 清除FIFO
    max30102.clearFIFO();
    
    sensor_initialized = true;
    Serial.println("[HR] MAX30102初始化成功");
    return true;
}

bool hr_available() {
    uint8_t status;
    if (!i2c_read(REG_INTR_STATUS_1, &status, 1)) return false;
    return (status & 0x40) != 0;  // PPG_RDY 位
}

bool hr_read_sample(int32_t* red, int32_t* ir) {
    uint8_t data[6];
    if (!i2c_read(REG_FIFO_DATA, data, 6)) return false;

    // 18-bit 有符号数，右对齐
    *red = ((int32_t)data[0] << 16 | (int32_t)data[1] << 8 | data[2]) >> 6;
    *ir  = ((int32_t)data[3] << 16 | (int32_t)data[4] << 8 | data[5]) >> 6;

    return true;
}

bool hr_read_latest(int32_t* red, int32_t* ir) {
    uint8_t wr_ptr, rd_ptr, ovf;
    uint8_t ptrs[3];

    // 一次性读出 WR_PTR、OVF_COUNTER、RD_PTR 三个寄存器
    if (!i2c_read(REG_FIFO_WR_PTR, ptrs, 3)) return false;
    wr_ptr = ptrs[0];
    ovf    = ptrs[1];
    rd_ptr = ptrs[2];

    // 如果有溢出，说明丢数据了（可记录日志）
    if (ovf > 0) {
        // 清除溢出计数器
        i2c_write(REG_OVF_COUNTER, 0);
    }

    int samples = (wr_ptr - rd_ptr + 32) % 32;  // FIFO 深度 32
    if (samples == 0) return false;

    // 只读最新一个样本（跳过旧数据）
    // 每个样本是 6 字节（RED 3字节 + IR 3字节）
    if (samples > 1) {
        // 设置读取指针到最新样本的前一个位置
        uint8_t new_rd_ptr = (wr_ptr - 1 + 32) % 32;
        if (!i2c_write(REG_FIFO_RD_PTR, new_rd_ptr)) {
            return false;
        }
    }

    // 读取最新的一个样本
    return hr_read_sample(red, ir);
}

void hr_shutdown() {
    i2c_write(REG_MODE_CONFIG, 0x80);  // SHDN = 1
}

void hr_wakeup() {
    i2c_write(REG_MODE_CONFIG, 0x03);  // 回到 SpO2 模式
}

float hr_read_temperature() {
    uint8_t buf[2];
    if (!i2c_write(REG_TEMP_CONFIG, 0x01)) return NAN;  // 启动单次测量
    delay(30);

    if (!i2c_read(REG_TEMP_INTEGER, buf, 2)) return NAN;

    int8_t  integer   = (int8_t)buf[0];
    uint8_t fraction  = buf[1];

    return (float)integer + (fraction >> 4) * 0.0625f + (fraction & 0x0F) * 0.00390625f;
}

#endif  // #if HR_USE_MOCK
