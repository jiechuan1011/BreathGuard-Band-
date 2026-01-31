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
// 真实 MAX30102 驱动

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

bool hr_init() {
    Wire.begin();

    // 1. 复位芯片
    if (!i2c_write(REG_MODE_CONFIG, 0x40)) {
        Serial.println("[HR] 复位失败");
        return false;
    }
    delay(100);

    // 2. 验证芯片 ID（可选，但强烈推荐）
    uint8_t part_id;
    if (!i2c_read(REG_PART_ID, &part_id, 1)) {
        Serial.println("[HR] 读取芯片ID失败");
        return false;
    }
    if (part_id != 0x15) {
        Serial.printf("[HR] 芯片ID错误: 0x%02X (期望0x15)\n", part_id);
        return false;
    }

    // 3. 配置 FIFO
    //    A_FULL = 0 (不产生几乎满中断)
    //    FIFO_ROLL = 1 (溢出后覆盖旧数据)
    uint8_t fifo_cfg = (HR_FIFO_AVERAGE << 5) | (1 << 4);  // avg + rollover
    if (!i2c_write(REG_FIFO_CONFIG, fifo_cfg)) {
        Serial.println("[HR] 配置FIFO失败");
        return false;
    }

    // 4. 模式：SpO2 + HR 模式 (0x03)
    if (!i2c_write(REG_MODE_CONFIG, 0x03)) {
        Serial.println("[HR] 设置模式失败");
        return false;
    }

    // 5. SpO2 配置
    //    SPO2_ADC_RGE = 00 (±2048nA, 18bit)
    //    SPO2_SR    = 根据 HR_SAMPLE_RATE 设置
    //    LED_PW     = 根据 HR_PULSE_WIDTH 设置
    uint8_t sr_code;
    switch (HR_SAMPLE_RATE) {
        case  50: sr_code = 0; break;
        case 100: sr_code = 1; break;
        case 200: sr_code = 2; break;
        case 400: sr_code = 3; break;
        default:  sr_code = 1; break;  // 默认 100Hz
    }

    uint8_t pw_code;
    switch (HR_PULSE_WIDTH) {
        case  69: pw_code = 0; break;
        case 118: pw_code = 1; break;
        case 215: pw_code = 2; break;
        case 411: pw_code = 3; break;
        default:  pw_code = 3; break;
    }

    uint8_t spo2_cfg = (0 << 5) | (sr_code << 2) | pw_code;
    if (!i2c_write(REG_SPO2_CONFIG, spo2_cfg)) {
        Serial.println("[HR] 配置SpO2失败");
        return false;
    }

    // 6. LED 电流（RED 和 IR）
    if (!i2c_write(REG_LED1_PA, HR_LED_CURRENT)) {
        Serial.println("[HR] 设置红光LED电流失败");
        return false;
    }
    if (!i2c_write(REG_LED2_PA, HR_LED_CURRENT)) {
        Serial.println("[HR] 设置红外LED电流失败");
        return false;
    }

    // 7. 清除 FIFO 指针
    if (!i2c_write(REG_FIFO_WR_PTR, 0x00)) {
        Serial.println("[HR] 清除写指针失败");
        return false;
    }
    if (!i2c_write(REG_FIFO_RD_PTR, 0x00)) {
        Serial.println("[HR] 清除读指针失败");
        return false;
    }

    // 8. 启用新 FIFO 数据中断
    if (!i2c_write(REG_INTR_ENABLE_1, 0x40)) {  // A_FULL_EN = 0, PPG_RDY_EN = 1
        Serial.println("[HR] 启用中断失败");
        return false;
    }

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
