/*
 * algorithm_manager_final.cpp - 算法集成与风险评估（最终版）
 * 
 * 功能：
 * 1. 调用hr_algorithm获取BPM/SpO2
 * 2. 运动校正（Kalman + TSSD）
 * 3. 风险评估（基于BPM/SpO2/SnO2组合）
 * 4. 输出AlgorithmResult + RiskAssessment
 */

#include <Arduino.h>
#include "../algorithm/hr_algorithm.h"
#include "../algorithm/motion_correction.h"
#include "algorithm_manager_final.h"
#include "sensor_collector_final.h"

// ==================== 全局算法状态 ====================

typedef struct {
    // HR算法
    uint8_t latest_bpm;
    uint8_t latest_spo2;
    uint8_t signal_quality;
    uint8_t correlation_quality;
    
    // SnO2/丙酮
    float acetone_ppm;
    uint16_t sno2_voltage_mv;
    
    // 运动校正
    KalmanState kalman_state;
    TssdState tssd_state;
    int16_t corrected_bpm;
    
    // 风险评估
    uint8_t risk_level;  // 0:低风险 1:中风险 2:高风险
    char risk_description[32];
    
    // 统计
    uint32_t total_updates;
    uint32_t last_update_ms;
} AlgorithmManagerState;

static AlgorithmManagerState g_alg = {0};

// ==================== 初始化 ====================

void algorithm_manager_init() {
    // 初始化HR算法
    hr_algorithm_init();
    
    // 初始化运动校正
    kalman_init(&g_alg.kalman_state, 70);  // 初始70 BPM
    tssd_init(&g_alg.tssd_state);
    
    g_alg.latest_bpm = 0;
    g_alg.latest_spo2 = 0;
    g_alg.signal_quality = 0;
    g_alg.acetone_ppm = 0.0f;
    g_alg.corrected_bpm = 0;
    
    strcpy(g_alg.risk_description, "正常");
    g_alg.risk_level = 0;
    
#ifdef DEBUG_MODE
    Serial.println("[ALG] 算法管理器初始化完成");
#endif
}

// ==================== HR算法更新（10ms周期） ====================

void algorithm_manager_update_hr() {
    // 调用hr_algorithm_update（自动从MAX30102采集）
    int status = hr_algorithm_update();
    
    // 尝试计算BPM
    int bpm_status = 0;
    uint8_t bpm = hr_calculate_bpm(&bpm_status);
    
    if (bpm > 0) {
        g_alg.latest_bpm = bpm;
        
        // 应用Kalman滤波
        int16_t corrected = kalman_update(&g_alg.kalman_state, (int16_t)bpm);
        // 应用TSSD（运动检测）
        corrected = tssd_update(&g_alg.tssd_state, corrected);
        g_alg.corrected_bpm = corrected;
    }
    
    // 尝试计算SpO2
    int spo2_status = 0;
    uint8_t spo2 = hr_calculate_spo2(&spo2_status);
    if (spo2 > 0) {
        g_alg.latest_spo2 = spo2;
    }
    
    g_alg.signal_quality = hr_get_signal_quality();
    g_alg.correlation_quality = hr_get_correlation_quality();
}

// ==================== SnO2采集（从传感器采集器） ====================

static void algorithm_update_sno2() {
    SensorSample sample = {0};
    if (sensor_collector_get_latest(SENSOR_TYPE_SNO2, &sample)) {
        g_alg.sno2_voltage_mv = sample.data.sno2.voltage_mv;
        
        // 简单线性转换：voltage → ppm（需根据实际标定调整）
        // 假设：0mV → 0ppm，3300mV → 100ppm
        g_alg.acetone_ppm = (sample.data.sno2.voltage_mv * 100.0f) / 3300.0f;
    }
}

// ==================== 风险评估 ====================

static void algorithm_assess_risk() {
    // 初始化为低风险
    g_alg.risk_level = 0;
    strcpy(g_alg.risk_description, "正常");
    
    // 风险判断逻辑（仅供参考，非医学诊断）
    uint8_t risk_factors = 0;
    
    // 因素1：心率异常（太快或太慢）
    if (g_alg.latest_bpm > 0) {
        if (g_alg.latest_bpm < 50 || g_alg.latest_bpm > 120) {
            risk_factors++;
        }
    }
    
    // 因素2：血氧低
    if (g_alg.latest_spo2 > 0 && g_alg.latest_spo2 < 95) {
        risk_factors++;
    }
    
    // 因素3：丙酮高（可能指示糖尿病风险）
    if (g_alg.acetone_ppm > 5.0f) {
        risk_factors += 2;  // 权重较高
    }
    
    // 因素4：信号质量差
    if (g_alg.signal_quality < 50) {
        risk_factors++;
    }
    
    // 综合评估
    if (risk_factors >= 3) {
        g_alg.risk_level = 2;
        strcpy(g_alg.risk_description, "高风险");
    } else if (risk_factors >= 1) {
        g_alg.risk_level = 1;
        strcpy(g_alg.risk_description, "中风险");
    } else {
        g_alg.risk_level = 0;
        strcpy(g_alg.risk_description, "正常");
    }
}

// ==================== 主更新（从调度器调用） ====================

void algorithm_manager_update() {
    algorithm_manager_update_hr();  // HR + 运动校正
    algorithm_update_sno2();        // SnO2 → 丙酮
    algorithm_assess_risk();        // 风险评估
    
    g_alg.total_updates++;
    g_alg.last_update_ms = millis();
}

// ==================== 结果查询 ====================

void algorithm_manager_get_result(AlgorithmResult* result) {
    if (result == NULL) return;
    
    result->timestamp_ms = millis();
    result->bpm = g_alg.latest_bpm;
    result->spo2 = g_alg.latest_spo2;
    result->corrected_bpm = (uint8_t)((g_alg.corrected_bpm / 256) & 0xFF);
    result->signal_quality = g_alg.signal_quality;
    result->correlation_quality = g_alg.correlation_quality;
    result->acetone_ppm = g_alg.acetone_ppm;
}

void algorithm_manager_get_risk_assessment(RiskAssessment* risk) {
    if (risk == NULL) return;
    
    risk->risk_level = g_alg.risk_level;
    strncpy(risk->risk_description, g_alg.risk_description, sizeof(risk->risk_description) - 1);
    risk->risk_description[sizeof(risk->risk_description) - 1] = '\0';
}

uint8_t algorithm_manager_has_valid_result() {
    return (g_alg.latest_bpm > 0 || g_alg.latest_spo2 > 0) ? 1 : 0;
}

void algorithm_manager_print_stats() {
#ifdef DEBUG_MODE
    Serial.printf("\n[ALG] BPM:%u SpO2:%u Acetone:%.1f RiskLevel:%u (%s)\n",
        g_alg.latest_bpm, g_alg.latest_spo2, g_alg.acetone_ppm,
        g_alg.risk_level, g_alg.risk_description);
#endif
}