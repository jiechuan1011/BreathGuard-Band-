/*
 * sensor_collector_final.cpp - 分时采集管理器（最终版）
 * 
 * 硬件映射（用户指定）：
 * - HR：MAX30102 @GPIO17/18 (I2C) → 100Hz采样
 * - SnO2：GPIO1 (ADC1_CH0) → AD623输出 → 10Hz采样
 * - 电池：GPIO2 (ADC1_CH1) → 60s采样
 * 
 * 优先级顺序实现：
 * 1. HR采集 + 运动校正（已整合hr_algorithm）
 * 2. SnO2 10Hz分时采集
 * 3. UI实时刷新
 * 4. BLE传输JSON
 */

#include <Arduino.h>
#include "../config/system_config.h"
#include "hr_driver.h"
#include "sno2_driver.h"
#include "sensor_collector_final.h"

// ==================== 引脚定义（用户确认） ====================
#define PIN_BAT_ADC              2          // GPIO2 ADC1_CH1
#define PIN_SNO2_ADC             1          // GPIO1 ADC1_CH0

// ADC配置
#define ADC_REF_MV               3300       // 参考电压 3.3V
#define ADC_RESOLUTION           4096       // 12位ADC

// ==================== 全局采集状态 ====================

typedef struct {
    // HR采集（100Hz）
    int32_t hr_red;
    int32_t hr_ir;
    uint32_t hr_last_read_ms;
    uint32_t hr_sample_count;
    
    // SnO2采集（10Hz）
    uint16_t sno2_raw_adc;
    uint32_t sno2_last_read_ms;
    uint32_t sno2_sample_count;
    
    // 电池采集（60s）
    uint16_t battery_mv;
    uint8_t battery_percent;
    uint32_t battery_last_read_ms;
    
    // 环形缓冲
    SensorSample sample_buffer[SENSOR_BUFFER_SIZE];
    uint16_t buffer_head;
    uint16_t buffer_tail;
    uint16_t buffer_count;
    
    // 统计
    uint32_t total_reads;
    uint32_t last_stats_ms;
} SensorCollectorState;

static SensorCollectorState g_collector = {0};

// ==================== 初始化 ====================

void sensor_collector_init() {
    memset(&g_collector, 0, sizeof(SensorCollectorState));
    
    // ADC初始化
    analogReadResolution(12);           // 12位分辨率
    adcAttachPin(PIN_BAT_ADC);          // 附加电池ADC
    adcAttachPin(PIN_SNO2_ADC);         // 附加SnO2 ADC
    
    // 时间戳初始化
    uint32_t now = millis();
    g_collector.hr_last_read_ms = now;
    g_collector.sno2_last_read_ms = now;
    g_collector.battery_last_read_ms = now;
    
#ifdef DEBUG_MODE
    Serial.println("[COLLECTOR] 初始化完成 - HR@100Hz SnO2@10Hz Battery@60s");
    Serial.printf("  HR缓冲: %d Hz × %d ms = %d样本\n", 100, 10, 100*10/1000);
    Serial.printf("  SnO2缓冲: %d Hz × %d ms = %d样本\n", 10, 100, 10*100/1000);
#endif
}

// ==================== 环形缓冲管理 ====================

static uint8_t sensor_buffer_push(const SensorSample* sample) {
    if (g_collector.buffer_count >= SENSOR_BUFFER_SIZE) {
        // 缓冲满，丢弃最旧数据
        g_collector.buffer_tail = (g_collector.buffer_tail + 1) % SENSOR_BUFFER_SIZE;
        g_collector.buffer_count--;
    }
    
    memcpy(&g_collector.sample_buffer[g_collector.buffer_head], 
           sample, sizeof(SensorSample));
    g_collector.buffer_head = (g_collector.buffer_head + 1) % SENSOR_BUFFER_SIZE;
    g_collector.buffer_count++;
    
    return 1;
}

// ==================== HR采集（10ms周期） ====================

static void sensor_collect_hr() {
    uint32_t now_ms = millis();
    
    // 10ms周期
    if ((now_ms - g_collector.hr_last_read_ms) < 10) {
        return;
    }
    
    if (hr_available()) {
        if (hr_read_latest(&g_collector.hr_red, &g_collector.hr_ir)) {
            SensorSample sample;
            sample.timestamp_ms = now_ms;
            sample.type = SENSOR_TYPE_HR;
            sample.data.hr.red = g_collector.hr_red;
            sample.data.hr.ir = g_collector.hr_ir;
            
            sensor_buffer_push(&sample);
            g_collector.hr_sample_count++;
            g_collector.hr_last_read_ms = now_ms;
            
#ifdef VERBOSE_COLLECTOR_DEBUG
            if (g_collector.hr_sample_count % 50 == 0) {
                Serial.printf("[HR] R:%ld IR:%ld (cnt:%lu)\n",
                    g_collector.hr_red, g_collector.hr_ir, g_collector.hr_sample_count);
            }
#endif
        }
    }
}

// ==================== SnO2采集（100ms周期） ====================

static void sensor_collect_sno2() {
    uint32_t now_ms = millis();
    
    // 100ms周期
    if ((now_ms - g_collector.sno2_last_read_ms) < 100) {
        return;
    }
    
    // 更新SnO2驱动状态机
    sno2_update();
    
    // 直接读取ADC（AD623输出）
    uint16_t adc_raw = analogRead(PIN_SNO2_ADC);
    uint16_t voltage_mv = (adc_raw * ADC_REF_MV) / ADC_RESOLUTION;
    
    SensorSample sample;
    sample.timestamp_ms = now_ms;
    sample.type = SENSOR_TYPE_SNO2;
    sample.data.sno2.voltage_mv = voltage_mv;
    sample.data.sno2.concentration_ppm = 0;  // 实际由sno2_driver计算
    sample.data.sno2.heater_on = sno2_is_heater_on();
    
    sensor_buffer_push(&sample);
    g_collector.sno2_sample_count++;
    g_collector.sno2_last_read_ms = now_ms;
    
#ifdef VERBOSE_COLLECTOR_DEBUG
    if (g_collector.sno2_sample_count % 10 == 0) {
        Serial.printf("[SnO2] ADC:%u mV:%u H:%u (cnt:%lu)\n",
            adc_raw, voltage_mv, sample.data.sno2.heater_on, g_collector.sno2_sample_count);
    }
#endif
}

// ==================== 电池采集（60s周期） ====================

static void sensor_collect_battery() {
    uint32_t now_ms = millis();
    
    // 60s周期
    if ((now_ms - g_collector.battery_last_read_ms) < 60000) {
        return;
    }
    
    // 读取电池ADC（GPIO2）
    uint16_t adc_raw = analogRead(PIN_BAT_ADC);
    g_collector.battery_mv = (adc_raw * ADC_REF_MV) / ADC_RESOLUTION;
    
    // 简单的百分比估算（根据实际电池特性调整）
    // 假设范围：2500mV (0%) ~ 4200mV (100%)
    if (g_collector.battery_mv <= 2500) {
        g_collector.battery_percent = 0;
    } else if (g_collector.battery_mv >= 4200) {
        g_collector.battery_percent = 100;
    } else {
        g_collector.battery_percent = 
            ((g_collector.battery_mv - 2500) * 100) / (4200 - 2500);
    }
    
    SensorSample sample;
    sample.timestamp_ms = now_ms;
    sample.type = SENSOR_TYPE_BATTERY;
    sample.data.battery.voltage_mv = g_collector.battery_mv;
    sample.data.battery.percent = g_collector.battery_percent;
    
    sensor_buffer_push(&sample);
    g_collector.battery_last_read_ms = now_ms;
    
#ifdef DEBUG_MODE
    Serial.printf("[BATTERY] %u mV (%.1fV) %u%% (raw:%u)\n",
        g_collector.battery_mv, g_collector.battery_mv / 1000.0f,
        g_collector.battery_percent, adc_raw);
#endif
}

// ==================== 主采集更新函数 ====================

void sensor_collector_update() {
    sensor_collect_hr();      // 10ms周期
    sensor_collect_sno2();    // 100ms周期
    sensor_collect_battery(); // 60s周期
    
    g_collector.total_reads++;
}

// ==================== 缓冲查询API ====================

uint16_t sensor_collector_available() {
    return g_collector.buffer_count;
}

uint8_t sensor_collector_read(SensorSample* sample) {
    if (sample == NULL || g_collector.buffer_count == 0) {
        return 0;
    }
    
    memcpy(sample, &g_collector.sample_buffer[g_collector.buffer_tail], 
           sizeof(SensorSample));
    g_collector.buffer_tail = (g_collector.buffer_tail + 1) % SENSOR_BUFFER_SIZE;
    g_collector.buffer_count--;
    
    return 1;
}

// 获取最新指定类型的样本
uint8_t sensor_collector_get_latest(SensorType type, SensorSample* sample) {
    if (sample == NULL) return 0;
    
    // 从缓冲末尾向前查找
    uint16_t idx = (g_collector.buffer_head - 1 + SENSOR_BUFFER_SIZE) % SENSOR_BUFFER_SIZE;
    for (uint16_t i = 0; i < g_collector.buffer_count; i++) {
        if (g_collector.sample_buffer[idx].type == type) {
            memcpy(sample, &g_collector.sample_buffer[idx], sizeof(SensorSample));
            return 1;
        }
        idx = (idx - 1 + SENSOR_BUFFER_SIZE) % SENSOR_BUFFER_SIZE;
    }
    return 0;
}

// ==================== 统计信息 ====================

void sensor_collector_get_stats(CollectorStats* stats) {
    if (stats == NULL) return;
    
    stats->total_hr_samples = g_collector.hr_sample_count;
    stats->total_sno2_samples = g_collector.sno2_sample_count;
    stats->battery_mv = g_collector.battery_mv;
    stats->battery_percent = g_collector.battery_percent;
    stats->buffer_count = g_collector.buffer_count;
    stats->total_reads = g_collector.total_reads;
}

void sensor_collector_print_stats() {
#ifdef DEBUG_MODE
    uint32_t now = millis();
    if ((now - g_collector.last_stats_ms) > 5000) {
        Serial.printf("\n[COLLECTOR STATS] HR:%lu SnO2:%lu Battery:%u%% Buffer:%u/%d\n",
            g_collector.hr_sample_count, g_collector.sno2_sample_count,
            g_collector.battery_percent, g_collector.buffer_count, SENSOR_BUFFER_SIZE);
        g_collector.last_stats_ms = now;
    }
#endif
}