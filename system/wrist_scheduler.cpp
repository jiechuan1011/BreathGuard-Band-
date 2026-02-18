#include "wrist_scheduler.h"
#include "../algorithm/hr_algorithm.h"
#include "sno2_driver.h"
#include "system_state.h"

// ──────────────────────────────────────────────
// 私有变量（低RAM优化）
// ──────────────────────────────────────────────

static SchedulerState current_state = SCHEDULER_STATE_INIT;
static TaskFlags task_flags = {0, 0, 0, 0, 0};
static SchedulerStats stats = {0, 0, 0, 0, 0, 0};

// 时间戳记录
static uint32_t last_hr_sample_time = 0;
static uint32_t last_hr_calc_time = 0;
static uint32_t last_sno2_sample_time = 0;
static uint32_t last_sno2_calc_time = 0;

// 初始化标志
static uint8_t initialized = 0;

// ──────────────────────────────────────────────
// 私有函数声明
// ──────────────────────────────────────────────

static void check_hr_sample_timing(uint32_t now);
static void check_hr_calc_timing(uint32_t now);
static void check_sno2_sample_timing(uint32_t now);
static void check_sno2_calc_timing(uint32_t now);
static void update_stats();

// ──────────────────────────────────────────────
// 公共函数实现
// ──────────────────────────────────────────────

void wrist_scheduler_init() {
    if (initialized) {
        return;
    }
    
    // 初始化算法模块
    hr_algorithm_init();
    
    // 初始化传感器驱动
    sno2_init();
    
    // 初始化系统状态
    system_state_init();
    
    // 记录初始时间
    uint32_t now = millis();
    last_hr_sample_time = now;
    last_hr_calc_time = now;
    last_sno2_sample_time = now;
    last_sno2_calc_time = now;
    
    // 设置初始状态
    current_state = SCHEDULER_STATE_RUNNING;
    initialized = 1;
    
    // 初始化统计
    stats.hr_samples = 0;
    stats.hr_calcs = 0;
    stats.sno2_samples = 0;
    stats.sno2_calcs = 0;
    stats.last_hr_bpm = 0;
    stats.last_sno2_ppm = 0;
}

void wrist_scheduler_update() {
    if (current_state != SCHEDULER_STATE_RUNNING) {
        return;
    }
    
    uint32_t now = millis();
    
    // 检查各个任务的定时
    check_hr_sample_timing(now);
    check_hr_calc_timing(now);
    check_sno2_sample_timing(now);
    check_sno2_calc_timing(now);
    
    // 更新统计
    update_stats();
}

SchedulerState wrist_scheduler_get_state() {
    return current_state;
}

TaskFlags wrist_scheduler_get_task_flags() {
    return task_flags;
}

void wrist_scheduler_clear_task_flags() {
    // 清除所有任务标志
    task_flags.hr_sample_due = 0;
    task_flags.hr_calc_due = 0;
    task_flags.sno2_sample_due = 0;
    task_flags.sno2_calc_due = 0;
}

SchedulerStats wrist_scheduler_get_stats() {
    return stats;
}

uint32_t wrist_scheduler_get_hr_sample_remaining() {
    uint32_t now = millis();
    uint32_t elapsed = now - last_hr_sample_time;
    
    if (elapsed >= HR_SAMPLE_INTERVAL_MS) {
        return 0;
    }
    
    return HR_SAMPLE_INTERVAL_MS - elapsed;
}

uint32_t wrist_scheduler_get_sno2_sample_remaining() {
    uint32_t now = millis();
    uint32_t elapsed = now - last_sno2_sample_time;
    
    if (elapsed >= SNO2_SAMPLE_INTERVAL_MS) {
        return 0;
    }
    
    return SNO2_SAMPLE_INTERVAL_MS - elapsed;
}

uint32_t wrist_scheduler_get_hr_calc_remaining() {
    uint32_t now = millis();
    uint32_t elapsed = now - last_hr_calc_time;
    
    if (elapsed >= HR_CALC_PERIOD_MS) {
        return 0;
    }
    
    return HR_CALC_PERIOD_MS - elapsed;
}

uint32_t wrist_scheduler_get_sno2_calc_remaining() {
    uint32_t now = millis();
    uint32_t elapsed = now - last_sno2_calc_time;
    
    if (elapsed >= SNO2_CALC_PERIOD_MS) {
        return 0;
    }
    
    return SNO2_CALC_PERIOD_MS - elapsed;
}

// ──────────────────────────────────────────────
// 私有函数实现
// ──────────────────────────────────────────────

static void check_hr_sample_timing(uint32_t now) {
    uint32_t elapsed = now - last_hr_sample_time;
    
    if (elapsed >= HR_SAMPLE_INTERVAL_MS) {
        // 设置MAX30102采样标志
        task_flags.hr_sample_due = 1;
        last_hr_sample_time = now;
        stats.hr_samples++;
    }
}

static void check_hr_calc_timing(uint32_t now) {
    uint32_t elapsed = now - last_hr_calc_time;
    
    if (elapsed >= HR_CALC_PERIOD_MS) {
        // 设置心率计算标志
        task_flags.hr_calc_due = 1;
        last_hr_calc_time = now;
        stats.hr_calcs++;
    }
}

static void check_sno2_sample_timing(uint32_t now) {
    uint32_t elapsed = now - last_sno2_sample_time;
    
    if (elapsed >= SNO2_SAMPLE_INTERVAL_MS) {
        // 设置SnO₂采样标志
        task_flags.sno2_sample_due = 1;
        last_sno2_sample_time = now;
        stats.sno2_samples++;
    }
}

static void check_sno2_calc_timing(uint32_t now) {
    uint32_t elapsed = now - last_sno2_calc_time;
    
    if (elapsed >= SNO2_CALC_PERIOD_MS) {
        // 设置SnO₂计算标志
        task_flags.sno2_calc_due = 1;
        last_sno2_calc_time = now;
        stats.sno2_calcs++;
    }
}

static void update_stats() {
    // 更新最后一次心率值
    uint8_t bpm = hr_get_latest_bpm();
    if (bpm > 0) {
        stats.last_hr_bpm = bpm;
    }
    
    // 更新最后一次丙酮浓度值
    Sno2Data sno2_data = sno2_get_data();
    if (sno2_data.valid) {
        stats.last_sno2_ppm = sno2_data.concentration_ppm;
    }
}

// ──────────────────────────────────────────────
// 外部任务处理示例（在main.cpp中使用）
// ──────────────────────────────────────────────
/*
void process_scheduler_tasks() {
    TaskFlags flags = wrist_scheduler_get_task_flags();
    
    if (flags.hr_sample_due) {
        // 处理MAX30102采样
        int status = hr_algorithm_update();
        if (status != HR_SUCCESS) {
            // 处理错误
        }
    }
    
    if (flags.hr_calc_due) {
        // 处理心率计算
        int calc_status;
        uint8_t bpm = hr_calculate_bpm(&calc_status);
        uint8_t spo2 = hr_calculate_spo2(&calc_status);
        uint8_t snr = hr_get_signal_quality();
        uint8_t correlation = hr_get_correlation_quality();
        
        // 更新系统状态
        system_state_set_hr_spo2(bpm, spo2, snr, correlation, calc_status);
    }
    
    if (flags.sno2_sample_due) {
        // 处理SnO₂采样（非阻塞更新）
        sno2_update();
    }
    
    if (flags.sno2_calc_due) {
        // 处理SnO₂计算
        Sno2Data data = sno2_get_data();
        if (data.valid) {
            // 更新系统状态
            system_state_set_sno2(data.voltage_mv, data.concentration_ppm, 1);
        } else {
            system_state_set_sno2(0, 0, 0);
        }
    }
    
    // 清除已处理的任务标志
    wrist_scheduler_clear_task_flags();
}
*/