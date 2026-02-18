#ifndef SENSOR_COLLECTOR_FINAL_H
#define SENSOR_COLLECTOR_FINAL_H

#include <Arduino.h>

#define SENSOR_BUFFER_SIZE      256

typedef enum {
    SENSOR_TYPE_HR = 0,
    SENSOR_TYPE_SNO2 = 1,
    SENSOR_TYPE_BATTERY = 2
} SensorType;

typedef struct {
    int32_t red;
    int32_t ir;
} HRData;

// Sno2Data 定义已集中在 drivers/sno2_driver.h 中，避免重复定义
#include "../drivers/sno2_driver.h"

typedef struct {
    uint16_t voltage_mv;
    uint8_t percent;
} BatteryData;

typedef struct {
    uint32_t timestamp_ms;
    SensorType type;
    union {
        HRData hr;
        Sno2Data sno2;
        BatteryData battery;
    } data;
} SensorSample;

typedef struct {
    uint32_t total_hr_samples;
    uint32_t total_sno2_samples;
    uint16_t battery_mv;
    uint8_t battery_percent;
    uint16_t buffer_count;
    uint32_t total_reads;
} CollectorStats;

void sensor_collector_init();
void sensor_collector_update();
uint16_t sensor_collector_available();
uint8_t sensor_collector_read(SensorSample* sample);
uint8_t sensor_collector_get_latest(SensorType type, SensorSample* sample);
void sensor_collector_get_stats(CollectorStats* stats);
void sensor_collector_print_stats();

#endif