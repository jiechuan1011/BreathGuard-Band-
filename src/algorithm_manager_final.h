#ifndef ALGORITHM_MANAGER_FINAL_H
#define ALGORITHM_MANAGER_FINAL_H

#include <Arduino.h>

typedef struct {
    uint32_t timestamp_ms;
    uint8_t bpm;
    uint8_t spo2;
    uint8_t corrected_bpm;
    uint8_t signal_quality;
    uint8_t correlation_quality;
    float acetone_ppm;
} AlgorithmResult;

typedef struct {
    uint8_t risk_level;  // 0:低 1:中 2:高
    char risk_description[32];
} RiskAssessment;

void algorithm_manager_init();
void algorithm_manager_update();
void algorithm_manager_get_result(AlgorithmResult* result);
void algorithm_manager_get_risk_assessment(RiskAssessment* risk);
uint8_t algorithm_manager_has_valid_result();
void algorithm_manager_print_stats();

#endif