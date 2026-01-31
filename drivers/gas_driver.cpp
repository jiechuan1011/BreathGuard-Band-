#include "gas_driver.h"
#include <math.h>

// ================= 私有变量 =================
static unsigned long warmup_start_ms = 0;
static bool warmup_complete = false;
static uint8_t heater_duty_cycle = GAS_HEATER_PREHEAT_DUTY;

// ================= 私有函数声明 =================
static uint16_t read_adc_raw();
static float adc_raw_to_mv(uint16_t raw);
static float calculate_variance(uint16_t* samples, uint8_t n);
static uint16_t median_filter(uint16_t* samples, uint8_t n);
static float calculate_resistance(float voltage_mv);
static float voltage_to_ppm(float voltage_mv);
static void update_heater_control();

// ================= 公共函数实现 =================

bool gas_init() {
#ifdef DEVICE_ROLE_DETECTOR
    GAS_DEBUG_PRINTLN("[GAS] 初始化气体传感器...");
    
    // 初始化ADC引脚
    pinMode(PIN_GAS_ADC, INPUT);
    
    // 初始化加热控制引脚（PWM）
#ifdef PIN_GAS_HEATER
    pinMode(PIN_GAS_HEATER, OUTPUT);
    // 设置PWM频率和分辨率（ESP32-C3）
    #ifdef ESP32
    ledcSetup(0, 1000, 8);  // 通道0，1kHz频率，8位分辨率
    ledcAttachPin(PIN_GAS_HEATER, 0);
    ledcWrite(0, (GAS_HEATER_PREHEAT_DUTY * 255) / 100);  // 初始预热占空比
    #else
    analogWrite(PIN_GAS_HEATER, (GAS_HEATER_PREHEAT_DUTY * 255) / 100);  // Arduino PWM
    #endif
    GAS_DEBUG_PRINTF("[GAS] 加热控制初始化完成，占空比: %d%%\n", GAS_HEATER_PREHEAT_DUTY);
#endif

    warmup_start_ms = millis();
    warmup_complete = false;
    heater_duty_cycle = GAS_HEATER_PREHEAT_DUTY;
    
    GAS_DEBUG_PRINTLN("[GAS] 气体传感器初始化完成，开始预热");
    return true;
#else
    // 腕带主控角色：初始化丙酮传感器接口，但返回true表示接口可用
    // 腕带没有实际传感器，但接口存在用于数据一致性
    GAS_DEBUG_PRINTLN("[GAS] 腕带模式：丙酮传感器接口初始化（无实际传感器）");
    warmup_complete = true; // 腕带不需要预热
    heater_duty_cycle = 0;  // 腕带没有加热
    return true;
#endif
}

bool gas_read(float* voltage_mv, float* conc_ppm) {
#ifdef DEVICE_ROLE_DETECTOR
    // 检查预热状态（非阻塞）
    if (!warmup_complete) {
        uint32_t elapsed = millis() - warmup_start_ms;
        if (elapsed < GAS_WARMUP_MS) {
            GAS_DEBUG_PRINTF("[GAS] 预热中... %lu/%lu ms\n", elapsed, GAS_WARMUP_MS);
            return false;  // 预热中，返回false
        }
        
        // 预热完成
        warmup_complete = true;
        heater_duty_cycle = GAS_HEATER_MAINTAIN_DUTY;
        update_heater_control();
        GAS_DEBUG_PRINTF("[GAS] 预热完成，切换加热占空比至: %d%%\n", heater_duty_cycle);
    }

    // 采集20个样本（间隔5ms，总采集时间100ms）
    uint16_t samples[GAS_SAMPLE_COUNT];
    for (uint8_t i = 0; i < GAS_SAMPLE_COUNT; i++) {
        samples[i] = read_adc_raw();
        delay(GAS_SAMPLE_INTERVAL_MS);
    }

    // 计算方差判断是否有运动干扰
    float variance = calculate_variance(samples, GAS_SAMPLE_COUNT);
    uint16_t processed_sample;
    
    if (variance > GAS_VARIANCE_THRESHOLD) {
        // 方差过大，使用中值滤波（抗运动干扰）
        processed_sample = median_filter(samples, GAS_SAMPLE_COUNT);
        GAS_DEBUG_PRINTF("[GAS] 运动干扰检测，使用中值滤波，方差: %.1f mV²\n", variance);
    } else {
        // 方差正常，使用平均值
        uint32_t sum = 0;
        for (uint8_t i = 0; i < GAS_SAMPLE_COUNT; i++) {
            sum += samples[i];
        }
        processed_sample = sum / GAS_SAMPLE_COUNT;
        GAS_DEBUG_PRINTF("[GAS] 正常采集，使用平均值，方差: %.1f mV²\n", variance);
    }

    // 转换为电压（mV）
    *voltage_mv = adc_raw_to_mv(processed_sample);
    
    // 转换为浓度（ppm）
    *conc_ppm = voltage_to_ppm(*voltage_mv);

    GAS_DEBUG_PRINTF("[GAS] 采集完成: %.1f mV -> %.2f ppm\n", *voltage_mv, *conc_ppm);
    return true;
#else
    // 腕带主控角色：返回0.0表示无效数据（数据一致性）
    *voltage_mv = 0.0;
    *conc_ppm = 0.0;
    return true; // 返回true表示接口可用，但数据为0.0
#endif
}

bool gas_is_warmed_up() {
    return warmup_complete;
}

float gas_get_heater_duty_cycle() {
    return heater_duty_cycle;
}

uint32_t gas_get_warmup_remaining() {
    if (warmup_complete) return 0;
    
    uint32_t elapsed = millis() - warmup_start_ms;
    if (elapsed >= GAS_WARMUP_MS) return 0;
    
    return GAS_WARMUP_MS - elapsed;
}

// ================= 私有函数实现 =================

// 读取ADC原始值
static uint16_t read_adc_raw() {
#ifdef DEVICE_ROLE_DETECTOR
    return analogRead(PIN_GAS_ADC) & 0xFFF;  // 12-bit掩码
#else
    return 0;
#endif
}

// ADC原始值转换为电压（mV）
static float adc_raw_to_mv(uint16_t raw) {
    return (raw / (float)GAS_ADC_RESOLUTION) * GAS_ADC_REF_MV;
}

// 计算样本方差（mV²）
static float calculate_variance(uint16_t* samples, uint8_t n) {
    if (n < 2) return 0.0f;
    
    // 计算平均值
    float sum = 0.0f;
    for (uint8_t i = 0; i < n; i++) {
        sum += adc_raw_to_mv(samples[i]);
    }
    float mean = sum / n;
    
    // 计算方差
    float variance = 0.0f;
    for (uint8_t i = 0; i < n; i++) {
        float diff = adc_raw_to_mv(samples[i]) - mean;
        variance += diff * diff;
    }
    variance /= (n - 1);
    
    return variance;
}

// 中值滤波（冒泡排序取中间值）
static uint16_t median_filter(uint16_t* samples, uint8_t n) {
    uint16_t temp[GAS_SAMPLE_COUNT];
    memcpy(temp, samples, n * sizeof(uint16_t));
    
    // 冒泡排序
    for (uint8_t i = 0; i < n - 1; i++) {
        for (uint8_t j = 0; j < n - i - 1; j++) {
            if (temp[j] > temp[j + 1]) {
                uint16_t swap = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = swap;
            }
        }
    }
    return temp[n / 2];
}

// 更新加热控制
static void update_heater_control() {
#ifdef PIN_GAS_HEATER
    #ifdef ESP32
    ledcWrite(0, (heater_duty_cycle * 255) / 100);
    #else
    analogWrite(PIN_GAS_HEATER, (heater_duty_cycle * 255) / 100);
    #endif
#endif
}

// 计算传感器电阻Rs（Ω）
// Rs = (Vcc - Vout) / Vout * RL
static float calculate_resistance(float voltage_mv) {
    if (voltage_mv <= 0) return 0.0f;
    
    float vout_v = voltage_mv / 1000.0f;
    float vcc_v = GAS_SUPPLY_VOLTAGE_MV / 1000.0f;
    
    // 防止除零
    if (vout_v < 0.001f) vout_v = 0.001f;
    
    float rs = ((vcc_v - vout_v) / vout_v) * GAS_LOAD_RESISTANCE;
    return rs;
}

// 电压转换为浓度（ppm）
// 对数模型：ppm = a * ln(Rs/R0) + b
// 其中：Rs/R0 = ratio = Rs / R0
// 使用基线比率：在清洁空气中，Rs/R0 = GAS_BASELINE_RATIO
static float voltage_to_ppm(float voltage_mv) {
    float rs = calculate_resistance(voltage_mv);
    if (rs <= 0) return 0.0f;
    
    // 计算清洁空气中的电阻R0
    // 根据基线比率：Rs_air = R0 * GAS_BASELINE_RATIO
    // 需要知道清洁空气中的电压Vair来计算Rs_air
    // 简化：假设清洁空气中电压为Vair = GAS_SUPPLY_VOLTAGE_MV / 2（典型值）
    // 实际应用中应通过校准获得准确的Vair
    
    float vair_mv = GAS_SUPPLY_VOLTAGE_MV / 2.0f;  // 假设清洁空气中电压
    float rs_air = calculate_resistance(vair_mv);
    if (rs_air <= 0) return 0.0f;
    
    // 计算R0
    float r0 = rs_air / GAS_BASELINE_RATIO;
    if (r0 <= 0) return 0.0f;
    
    // 计算Rs/R0比率
    float ratio = rs / r0;
    if (ratio <= 0) return 0.0f;
    
    // 对数模型转换
    float ppm = GAS_CALIB_A * log(ratio) + GAS_CALIB_B;
    
    // 限制有效范围
    if (ppm < 0) ppm = 0.0f;
    
    GAS_DEBUG_PRINTF("[GAS] 浓度转换: Rs=%.0fΩ, R0=%.0fΩ, ratio=%.3f, ppm=%.2f\n", 
                     rs, r0, ratio, ppm);
    return ppm;
}
