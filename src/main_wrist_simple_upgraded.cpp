// 家用糖尿病初筛腕带简化主程序（ESP32-C3 SuperMini）
// 基于现有hr_algorithm.cpp的低RAM优化版升级
// 修复了编译错误，使用Serial.print代替Serial.printf

#include <Arduino.h>
#include "../system/wrist_scheduler.h"
#include "../algorithm/hr_algorithm.h"
#include "../drivers/sno2_driver.h"

// ──────────────────────────────────────────────
// 简化调试输出（避免printf问题）
// ──────────────────────────────────────────────

#define DEBUG_ENABLED 1

#if DEBUG_ENABLED
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// 简化版printf替代函数
void debug_printf(const char* format, ...) {
#if DEBUG_ENABLED
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.print(buffer);
#endif
}

// ──────────────────────────────────────────────
// 函数声明
// ──────────────────────────────────────────────

void setup();
void loop();
void process_scheduler_tasks();

// ──────────────────────────────────────────────
// 主程序
// ──────────────────────────────────────────────

void setup() {
    // 初始化串口调试
#if DEBUG_ENABLED
    Serial.begin(115200);
    delay(1000);
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("家用糖尿病初筛腕带 v2.0 (简化版)");
    DEBUG_PRINTLN("基于ESP32-C3 SuperMini（低RAM优化）");
    DEBUG_PRINTLN("========================================");
#endif

    // 初始化调度器（会自动初始化所有传感器和算法）
    wrist_scheduler_init();
    
    DEBUG_PRINTLN("系统初始化完成");
    DEBUG_PRINTLN("开始分时采集调度...");
    
    // 设置SnO₂标定参数（示例值，需要实际校准）
    // ppm = a * voltage_mv + b
    sno2_set_calibration(0.5f, -100.0f);
}

void loop() {
    // 1. 更新调度器（非阻塞）
    wrist_scheduler_update();
    
    // 2. 处理调度器产生的任务
    process_scheduler_tasks();
    
    // 3. 短暂延时，避免CPU占用过高
    delay(1);  // 1ms延时，足够响应10ms的MAX30102采样
}

// ──────────────────────────────────────────────
// 任务处理函数
// ──────────────────────────────────────────────

void process_scheduler_tasks() {
    // 获取任务标志
    TaskFlags flags = wrist_scheduler_get_task_flags();
    
    // 处理MAX30102采样任务
    if (flags.hr_sample_due) {
        int status = hr_algorithm_update();
        if (status != HR_SUCCESS) {
            DEBUG_PRINT("[HR] 采样失败: ");
            DEBUG_PRINTLN(status);
        }
    }
    
    // 处理心率计算任务
    if (flags.hr_calc_due) {
        int calc_status;
        uint8_t bpm = hr_calculate_bpm(&calc_status);
        uint8_t spo2 = hr_calculate_spo2(&calc_status);
        uint8_t snr = hr_get_signal_quality();
        uint8_t correlation = hr_get_correlation_quality();
        
        if (calc_status == HR_SUCCESS || calc_status == HR_SUCCESS_WITH_MOTION) {
            DEBUG_PRINT("[HR] BPM: ");
            DEBUG_PRINT(bpm);
            DEBUG_PRINT(", SpO2: ");
            DEBUG_PRINT(spo2);
            DEBUG_PRINT("%, SNR: ");
            DEBUG_PRINT(snr / 10);
            DEBUG_PRINT(".");
            DEBUG_PRINT(snr % 10);
            DEBUG_PRINT(", Corr: ");
            DEBUG_PRINT(correlation);
            DEBUG_PRINTLN("%");
        } else {
            DEBUG_PRINT("[HR] 计算失败: ");
            DEBUG_PRINTLN(calc_status);
        }
    }
    
    // 处理SnO₂采样任务
    if (flags.sno2_sample_due) {
        // 非阻塞更新SnO₂传感器状态机
        sno2_update();
        
        // 获取SnO₂状态
        Sno2State state = sno2_get_state();
        if (state == SNO2_STATE_HEATING) {
            uint32_t remaining = sno2_get_heating_remaining();
            DEBUG_PRINT("[SnO2] 加热中，剩余: ");
            DEBUG_PRINT(remaining);
            DEBUG_PRINTLN(" ms");
        } else if (state == SNO2_STATE_SAMPLING) {
            DEBUG_PRINTLN("[SnO2] 采样中...");
        }
    }
    
    // 处理SnO₂计算任务
    if (flags.sno2_calc_due) {
        Sno2Data data = sno2_get_data();
        if (data.valid) {
            DEBUG_PRINT("[SnO2] 电压: ");
            DEBUG_PRINT(data.voltage_mv);
            DEBUG_PRINT(" mV, 浓度: ");
            DEBUG_PRINT(data.concentration_ppm);
            DEBUG_PRINTLN(" ppm");
            
            // 检查丙酮浓度阈值（糖尿病初筛）
            if (data.concentration_ppm > 50) {  // 示例阈值，需根据实际校准
                DEBUG_PRINTLN("[警告] 丙酮浓度升高，建议进一步检查");
            }
        } else {
            DEBUG_PRINTLN("[SnO2] 数据无效（预热中或采样失败）");
        }
    }
    
    // 清除已处理的任务标志
    wrist_scheduler_clear_task_flags();
    
    // 每5秒显示一次系统状态
    static uint32_t last_status_time = 0;
    uint32_t now = millis();
    if (now - last_status_time > 5000) {
        // 获取调度器统计
        SchedulerStats stats = wrist_scheduler_get_stats();
        
        DEBUG_PRINT("[状态] HR采样: ");
        DEBUG_PRINT(stats.hr_samples);
        DEBUG_PRINT(", 计算: ");
        DEBUG_PRINT(stats.hr_calcs);
        DEBUG_PRINT(", SnO2采样: ");
        DEBUG_PRINT(stats.sno2_samples);
        DEBUG_PRINT(", 计算: ");
        DEBUG_PRINTLN(stats.sno2_calcs);
        
        // 获取剩余时间
        uint32_t hr_remaining = wrist_scheduler_get_hr_sample_remaining();
        uint32_t sno2_remaining = wrist_scheduler_get_sno2_sample_remaining();
        
        DEBUG_PRINT("[定时] 下次HR采样: ");
        DEBUG_PRINT(hr_remaining);
        DEBUG_PRINT(" ms, 下次SnO2采样: ");
        DEBUG_PRINT(sno2_remaining);
        DEBUG_PRINTLN(" ms");
        
        last_status_time = now;
    }
}

// ──────────────────────────────────────────────
// 电源管理示例（可选）
// ──────────────────────────────────────────────

/*
// 如果需要低功耗，可以添加light sleep
#include "esp_sleep.h"

void enter_light_sleep(uint32_t ms) {
    if (ms > 0) {
        esp_sleep_enable_timer_wakeup(ms * 1000);
        esp_light_sleep_start();
    }
}

void loop_low_power() {
    // 更新调度器
    wrist_scheduler_update();
    
    // 计算到下一个事件的时间
    uint32_t next_hr = wrist_scheduler_get_hr_sample_remaining();
    uint32_t next_sno2 = wrist_scheduler_get_sno2_sample_remaining();
    uint32_t next_event = min(next_hr, next_sno2);
    
    // 如果下一个事件在10ms以上，进入light sleep
    if (next_event > 10) {
        // 提前1ms唤醒，确保及时处理
        enter_light_sleep(next_event - 1);
    }
    
    // 处理任务
    process_scheduler_tasks();
}
*/