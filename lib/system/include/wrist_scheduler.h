// Copied from system/wrist_scheduler.h for LDF
#ifndef WRIST_SCHEDULER_H
#define WRIST_SCHEDULER_H

#include <Arduino.h>

// 采样节奏配置
#define HR_SAMPLE_INTERVAL_MS     10      // MAX30102采样间隔（10ms = 100Hz）
#define SNO2_SAMPLE_INTERVAL_MS   40000   // SnO₂采样间隔（40秒）

// 计算周期配置
#define HR_CALC_PERIOD_MS         2000    // 心率计算周期（2秒）
#define SNO2_CALC_PERIOD_MS       40000   // SnO₂计算周期（40秒）

typedef enum {
    SCHEDULER_STATE_INIT = 0,
    SCHEDULER_STATE_RUNNING,
    SCHEDULER_STATE_ERROR
} SchedulerState;

typedef struct {
    uint8_t hr_sample_due : 1;
    uint8_t hr_calc_due : 1;
    uint8_t sno2_sample_due : 1;
    uint8_t sno2_calc_due : 1;
    uint8_t reserved : 4;
} TaskFlags;

typedef struct {
    uint32_t hr_samples;
    uint32_t hr_calcs;
    uint32_t sno2_samples;
    uint32_t sno2_calcs;
    uint32_t last_hr_bpm;
    uint32_t last_sno2_ppm;
} SchedulerStats;

void wrist_scheduler_init();
void wrist_scheduler_update();
SchedulerState wrist_scheduler_get_state();
TaskFlags wrist_scheduler_get_task_flags();
void wrist_scheduler_clear_task_flags();
SchedulerStats wrist_scheduler_get_stats();
uint32_t wrist_scheduler_get_hr_sample_remaining();
uint32_t wrist_scheduler_get_sno2_sample_remaining();
uint32_t wrist_scheduler_get_hr_calc_remaining();
uint32_t wrist_scheduler_get_sno2_calc_remaining();

#endif // WRIST_SCHEDULER_H
