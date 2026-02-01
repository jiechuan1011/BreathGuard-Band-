#include "sno2_driver.h"

// ──────────────────────────────────────────────
// 私有变量（低RAM优化）
// ──────────────────────────────────────────────

static Sno2State current_state = SNO2_STATE_IDLE;
static Sno2Data current_data = {0, 0, 0, 0, 0};
static uint32_t last_cycle_start = 0;
static uint32_t heater_start_time = 0;
static uint32_t sample_start_time = 0;
static uint16_t adc_samples[SNO2_ADC_SAMPLES];
static uint8_t sample_count = 0;

// 标定参数（Q10.6格式）
static int16_t calib_a_q = SNO2_CALIB_A_Q;
static int16_t calib_b_q = SNO2_CALIB_B_Q;

// ──────────────────────────────────────────────
// 私有函数声明
// ──────────────────────────────────────────────

static void enter_idle_state();
static void enter_heating_state();
static void enter_sampling_state();
static void enter_calculating_state();
static void enter_error_state();

static uint16_t read_adc_raw();
static uint16_t calculate_average_adc();
static uint16_t adc_raw_to_mv(uint16_t raw);
static uint16_t calculate_concentration(uint16_t voltage_mv);
static void update_heater(uint8_t state);

// ──────────────────────────────────────────────
// 公共函数实现
// ──────────────────────────────────────────────

void sno2_init() {
    // 初始化引脚
    pinMode(SNO2_HEATER_PIN, OUTPUT);
    pinMode(SNO2_ADC_PIN, INPUT);
    
    // 初始状态
    digitalWrite(SNO2_HEATER_PIN, SNO2_HEATER_OFF);
    current_state = SNO2_STATE_IDLE;
    current_data.heater_on = 0;
    current_data.valid = 0;
    
    // 记录第一个周期开始时间
    last_cycle_start = millis();
    
    // 进入空闲状态，等待第一个加热周期
    enter_idle_state();
}

void sno2_update() {
    uint32_t now = millis();
    uint32_t cycle_elapsed = now - last_cycle_start;
    
    switch (current_state) {
        case SNO2_STATE_IDLE:
            // 检查是否到达加热周期（每40秒）
            if (cycle_elapsed >= SNO2_CYCLE_INTERVAL_MS) {
                enter_heating_state();
            }
            break;
            
        case SNO2_STATE_HEATING:
            // 检查加热是否完成（8秒）
            if (now - heater_start_time >= SNO2_HEAT_DURATION_MS) {
                enter_sampling_state();
            }
            break;
            
        case SNO2_STATE_SAMPLING:
            // 每10ms采集一个样本
            static uint32_t last_sample_time = 0;
            if (now - last_sample_time >= 10) {  // 10ms采样间隔
                if (sample_count < SNO2_ADC_SAMPLES) {
                    adc_samples[sample_count] = read_adc_raw();
                    sample_count++;
                    last_sample_time = now;
                }
                
                // 检查是否采集完成
                if (sample_count >= SNO2_ADC_SAMPLES) {
                    enter_calculating_state();
                }
            }
            break;
            
        case SNO2_STATE_CALCULATING:
            // 计算平均值和浓度
            {
                uint16_t avg_adc = calculate_average_adc();
                uint16_t voltage_mv = adc_raw_to_mv(avg_adc);
                uint16_t concentration = calculate_concentration(voltage_mv);
                
                // 更新数据
                current_data.voltage_mv = voltage_mv;
                current_data.concentration_ppm = concentration;
                current_data.valid = 1;
                
                // 重置周期
                last_cycle_start = now;
                sample_count = 0;
                
                // 返回空闲状态
                enter_idle_state();
            }
            break;
            
        case SNO2_STATE_ERROR:
            // 错误状态，需要外部干预
            break;
    }
}

Sno2State sno2_get_state() {
    return current_state;
}

Sno2Data sno2_get_data() {
    return current_data;
}

void sno2_set_calibration(float a, float b) {
    // 将浮点参数转换为Q10.6格式
    calib_a_q = (int16_t)(a * SNO2_Q_SCALE);
    calib_b_q = (int16_t)(b * SNO2_Q_SCALE);
}

uint8_t sno2_is_heater_on() {
    return current_data.heater_on;
}

uint32_t sno2_get_heating_remaining() {
    if (current_state != SNO2_STATE_HEATING) {
        return 0;
    }
    
    uint32_t now = millis();
    uint32_t elapsed = now - heater_start_time;
    
    if (elapsed >= SNO2_HEAT_DURATION_MS) {
        return 0;
    }
    
    return SNO2_HEAT_DURATION_MS - elapsed;
}

uint32_t sno2_get_next_sample_time() {
    uint32_t now = millis();
    uint32_t cycle_elapsed = now - last_cycle_start;
    
    if (cycle_elapsed >= SNO2_CYCLE_INTERVAL_MS) {
        return 0;  // 应该立即采样
    }
    
    return SNO2_CYCLE_INTERVAL_MS - cycle_elapsed;
}

// ──────────────────────────────────────────────
// 私有函数实现
// ──────────────────────────────────────────────

static void enter_idle_state() {
    current_state = SNO2_STATE_IDLE;
    update_heater(SNO2_HEATER_OFF);
}

static void enter_heating_state() {
    current_state = SNO2_STATE_HEATING;
    heater_start_time = millis();
    update_heater(SNO2_HEATER_ON);
}

static void enter_sampling_state() {
    current_state = SNO2_STATE_SAMPLING;
    sample_start_time = millis();
    sample_count = 0;
    update_heater(SNO2_HEATER_OFF);  // 采样时关闭加热
}

static void enter_calculating_state() {
    current_state = SNO2_STATE_CALCULATING;
}

static void enter_error_state() {
    current_state = SNO2_STATE_ERROR;
    update_heater(SNO2_HEATER_OFF);
}

static uint16_t read_adc_raw() {
    // 读取ADC原始值（12位）
    return analogRead(SNO2_ADC_PIN) & 0xFFF;
}

static uint16_t calculate_average_adc() {
    // 使用移位快速计算平均值（样本数必须是2的幂次）
    uint32_t sum = 0;
    for (uint8_t i = 0; i < SNO2_ADC_SAMPLES; i++) {
        sum += adc_samples[i];
    }
    
    // 除以16（右移4位）
    return (uint16_t)(sum >> 4);
}

static uint16_t adc_raw_to_mv(uint16_t raw) {
    // 定点运算：voltage_mv = raw * 3300 / 4096
    // 使用32位中间值避免溢出
    uint32_t voltage = (uint32_t)raw * SNO2_ADC_REF_MV;
    return (uint16_t)(voltage / SNO2_ADC_RESOLUTION);
}

static uint16_t calculate_concentration(uint16_t voltage_mv) {
    // 使用Q格式定点运算：ppm = a * voltage + b
    // 步骤：1. voltage * a (Q10.6 * Q10.6 = Q20.12)
    //       2. 右移6位得到Q20.6
    //       3. 加上b (Q10.6)
    //       4. 右移6位得到整数ppm
    
    // 转换为int32_t进行中间计算
    int32_t voltage_q = (int32_t)voltage_mv << SNO2_Q_FRACTION_BITS;  // 转换为Q10.6
    
    // 计算 a * voltage (Q10.6 * Q10.6 = Q20.12)
    int32_t product = (int32_t)calib_a_q * voltage_q;  // Q20.12
    
    // 右移6位得到Q20.6
    product >>= SNO2_Q_FRACTION_BITS;  // Q20.6
    
    // 加上b (Q10.6转换为Q20.6)
    int32_t b_q20 = (int32_t)calib_b_q << SNO2_Q_FRACTION_BITS;  // Q20.6
    product += b_q20;  // Q20.6
    
    // 右移6位得到整数ppm
    int32_t ppm = product >> SNO2_Q_FRACTION_BITS;  // 整数
    
    // 限制范围
    if (ppm < SNO2_CONC_MIN_PPM) ppm = SNO2_CONC_MIN_PPM;
    if (ppm > SNO2_CONC_MAX_PPM) ppm = SNO2_CONC_MAX_PPM;
    
    return (uint16_t)ppm;
}

static void update_heater(uint8_t state) {
    digitalWrite(SNO2_HEATER_PIN, state);
    current_data.heater_on = state;
}