/*
 * 极简测试版本 - 仅MAX30102心率采集
 * RAM使用: <200 bytes
 * 用途: 验证硬件连接和基本功能
 */

#include <Arduino.h>
#include "../drivers/hr_driver.h"

// 极简配置
#define SAMPLE_INTERVAL_MS  10
#define CALC_INTERVAL_MS    2000

// 最小缓冲区（仅16样本，约0.16秒）
#define MIN_BUFFER_SIZE     16
static int16_t ir_samples[MIN_BUFFER_SIZE];
static uint8_t sample_pos = 0;
static bool buffer_ready = false;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    
    Serial.println(F("=== 极简心率测试 ==="));
    
    if (!hr_init()) {
        Serial.println(F("MAX30102初始化失败！"));
        while(1);
    }
    
    Serial.println(F("开始采集..."));
}

void loop() {
    static unsigned long last_sample = 0;
    static unsigned long last_calc = 0;
    unsigned long now = millis();
    
    // 采集样本
    if (now - last_sample >= SAMPLE_INTERVAL_MS) {
        int32_t red, ir;
        if (hr_read_latest(&red, &ir)) {
            ir_samples[sample_pos] = (int16_t)(ir >> 2);
            sample_pos = (sample_pos + 1) % MIN_BUFFER_SIZE;
            if (sample_pos == 0) buffer_ready = true;
        }
        last_sample = now;
    }
    
    // 简单计算（每2秒）
    if (buffer_ready && (now - last_calc >= CALC_INTERVAL_MS)) {
        // 找最大值和最小值（简单峰值检测）
        int16_t min_val = 32767, max_val = -32768;
        for (uint8_t i = 0; i < MIN_BUFFER_SIZE; i++) {
            if (ir_samples[i] < min_val) min_val = ir_samples[i];
            if (ir_samples[i] > max_val) max_val = ir_samples[i];
        }
        
        int16_t amplitude = max_val - min_val;
        
        Serial.print(F("幅度: "));
        Serial.print(amplitude);
        Serial.print(F(" | 范围: ["));
        Serial.print(min_val);
        Serial.print(F(", "));
        Serial.print(max_val);
        Serial.println(F("]"));
        
        // 简单BPM估算（基于幅度变化频率）
        if (amplitude > 100) {
            Serial.println(F("检测到心跳信号"));
        } else {
            Serial.println(F("信号弱，请调整手指位置"));
        }
        
        last_calc = now;
    }
}
