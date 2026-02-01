#ifndef WRIST_SCHEDULER_H
#define WRIST_SCHEDULER_H

#include <Arduino.h>

// ──────────────────────────────────────────────
// 分时采集调度器配置（非阻塞，低RAM优化）
// ──────────────────────────────────────────────

// 采样节奏配置
#define HR_SAMPLE_INTERVAL_MS     10      // MAX30102采样间隔（10ms = 100Hz）
#define SNO2_SAMPLE_INTERVAL_MS   40000   // SnO₂采样间隔（40秒）

// 计算周期配置
#define HR_CALC_PERIOD_MS         2000    // 心率计算周期（2秒）
#define SNO2_CALC_PERIOD_MS       40000   // SnO₂计算周期（40秒，与采样同步）

// 状态定义
typedef enum {
    SCHEDULER_STATE_INIT = 0,
    SCHEDULER_STATE_RUNNING,
    SCHEDULER_STATE_ERROR
} SchedulerState;

// 任务标志
typedef struct {
    uint8_t hr_sample_due : 1;     // MAX30102采样到期
    uint8_t hr_calc_due : 1;       // 心率计算到期
    uint8_t sno2_sample_due : 1;   // SnO₂采样到期
    uint8_t sno2_calc_due : 1;     // SnO₂计算到期
    uint8_t reserved : 4;
} TaskFlags;

// 调度器统计
typedef struct {
    uint32_t hr_samples;           // MAX30102采样次数
    uint32_t hr_calcs;             // 心率计算次数
    uint32_t sno2_samples;         // SnO₂采样次数
    uint32_t sno2_calcs;           // SnO₂计算次数
    uint32_t last_hr_bpm;          // 最后一次心率（BPM）
    uint32_t last_sno2_ppm;        // 最后一次丙酮浓度（ppm）
} SchedulerStats;

// ──────────────────────────────────────────────
// 函数声明
// ──────────────────────────────────────────────

// 初始化调度器
void wrist_scheduler_init();

// 非阻塞调度器更新（在loop()中持续调用）
void wrist_scheduler_update();

// 获取当前状态
SchedulerState wrist_scheduler_get_state();

// 获取任务标志（用于外部任务处理）
TaskFlags wrist_scheduler_get_task_flags();

// 清除任务标志（任务完成后调用）
void wrist_scheduler_clear_task_flags();

// 获取调度器统计
SchedulerStats wrist_scheduler_get_stats();

// 获取下一次MAX30102采样剩余时间（ms）
uint32_t wrist_scheduler_get_hr_sample_remaining();

// 获取下一次SnO₂采样剩余时间（ms）
uint32_t wrist_scheduler_get_sno2_sample_remaining();

// 获取下一次心率计算剩余时间（ms）
uint32_t wrist_scheduler_get_hr_calc_remaining();

// 获取下一次SnO₂计算剩余时间（ms）
uint32_t wrist_scheduler_get_sno2_calc_remaining();


