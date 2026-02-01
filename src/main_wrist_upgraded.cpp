// 家用糖尿病初筛腕带主程序（ESP32-C3 SuperMini）
// 基于现有hr_algorithm.cpp的低RAM优化版升级
// 新增功能：
// 1. SnO₂气敏传感器采集（MQ-138/TGS822）
// 2. 运动干扰校正（Kalman滤波/TSSD）
// 3. 分时采集主循环框架（非阻塞）

#include <Arduino.h>
#include "../system/wrist_scheduler.h"
#include "../algorithm/hr_algorithm.h"
#include "../drivers/sno2_driver.h"
#include "../system/system_state.h"
#include "../config/pin_config.h"

// ──────────────────────────────────────────────
// 全局变量
// ──────────────────────────────────────────────

// 调试开关
#define DEBUG_ENABLED 1

#if DEBUG_ENABLED
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#endif

// ──────────────────────────────────────────────
// 函数声明
// ──────────────────────────────────────────────

void setup();
void loop();
void process_scheduler_tasks();
void update_display();
void handle_user_input();
void send_data_via_ble();

// ──────────────────────────────────────────────
// 主程序
// ──────────────────────────────────────────────

void setup() {
    // 初始化串口调试
#if DEBUG_ENABLED
    Serial.begin(115200);
    delay(1000);
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("家用糖尿病初筛腕带 v2.0");
    DEBUG_PRINTLN("基于ESP32-C3 SuperMini（低RAM优化）");
    DEBUG_PRINTLN("========================================");
#endif

    // 初始化调度器（会自动初始化所有传感器和算法）
    wrist_scheduler_init();
    
    DEBUG_PRINTLN("系统初始化完成");
    DEBUG_PRINTLN("开始分时采集调度...");
}

void loop() {
    // 1. 更新调度器（非阻塞）
    wrist_scheduler_update();
    
    // 2. 处理调度器产生的任务
    process_scheduler_tasks();
    
    // 3. 处理用户输入（按键等）
    handle_user_input();
    
    // 4. 更新显示（如果有OLED）
    update_display();
    
    // 5. 通过BLE发送数据
    send_data_via_ble();
    
    // 6. 短暂延时，避免CPU占用过高
    delay(1);  // 1ms延时，足够响应10ms的MAX30102采样
}

// ──────────────────────────────────────────────
// 任务处理函数
// ──────────────────────────────────────────────

void process_scheduler_tasks() {
    TaskFlags flags = wrist_scheduler_get_task_flags();
    
    // 处理MAX30102采样任务
    if (flags.hr_sample_due) {
        int status = hr_algorithm_update();
        if (status != HR_SUCCESS) {
            DEBUG_PRINTF("[HR] 采样失败: %d\n", status);
        }
    }
    
    // 处理心率计算任务
    if (flags.hr_calc_due) {
        int calc_status;
        uint8_t bpm = hr_calculate_bpm(&calc_status);
        uint8_t spo2 = hr_calculate_spo2(&calc_status);
        uint8_t snr = hr_get_signal_quality();
        uint8_t correlation = hr_get_correlation_quality();
        
        // 更新系统状态
        system_state_set_hr_spo2(bpm, spo2, snr, correlation, calc_status);
        
        if (calc_status == HR_SUCCESS || calc_status == HR_SUCCESS_WITH_MOTION) {
            DEBUG_PRINTF("[HR] BPM: %d, SpO2: %d%%, SNR: %d.%d, Corr: %d%%\n",
                        bpm, spo2, snr/10, snr%10, correlation);
        } else {
            DEBUG_PRINTF("[HR] 计算失败: %d\n", calc_status);
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
            DEBUG_PRINTF("[SnO2] 加热中，剩余: %lu ms\n", remaining);
        } else if (state == SNO2_STATE_SAMPLING) {
            DEBUG_PRINTLN("[SnO2] 采样中...");
        }
    }
    
    // 处理SnO₂计算任务
    if (flags.sno2_calc_due) {
        Sno2Data data = sno2_get_data();
        if (data.valid) {
            // 更新系统状态
            system_state_set_sno2(data.voltage_mv, data.concentration_ppm, 1);
            
            DEBUG_PRINTF("[SnO2] 电压: %d mV, 浓度: %d ppm\n",
                        data.voltage_mv, data.concentration_ppm);
            
            // 检查丙酮浓度阈值（糖尿病初筛）
            if (data.concentration_ppm > 50) {  // 示例阈值，需根据实际校准
                DEBUG_PRINTLN("[警告] 丙酮浓度升高，建议进一步检查");
            }
        } else {
            system_state_set_sno2(0, 0, 0);
            DEBUG_PRINTLN("[SnO2] 数据无效（预热中或采样失败）");
        }
    }
    
    // 清除已处理的任务标志
    wrist_scheduler_clear_task_flags();
}

// ──────────────────────────────────────────────
// 用户输入处理
// ──────────────────────────────────────────────

void handle_user_input() {
    // 示例：检查按键输入
    static uint32_t last_button_check = 0;
    uint32_t now = millis();
    
    if (now - last_button_check > 200) {  // 每200ms检查一次
        // 检查按键1（切换显示/确认）
        if (digitalRead(PIN_BTN1) == LOW) {
            DEBUG_PRINTLN("[UI] 按键1按下");
            // 切换显示模式或确认操作
        }
        
        // 检查按键2（返回/取消）
        if (digitalRead(PIN_BTN2) == LOW) {
            DEBUG_PRINTLN("[UI] 按键2按下");
            // 返回上一级或取消操作
        }
        
        last_button_check = now;
    }
}

// ──────────────────────────────────────────────
// 显示更新
// ──────────────────────────────────────────────

void update_display() {
    // 示例：更新OLED显示
    // 实际实现需要根据具体的显示驱动库
    
    static uint32_t last_display_update = 0;
    uint32_t now = millis();
    
    // 每500ms更新一次显示
    if (now - last_display_update > 500) {
        // 获取当前系统状态
        // 显示心率、血氧、丙酮浓度等信息
        
        last_display_update = now;
    }
}

// ──────────────────────────────────────────────
// BLE数据传输
// ──────────────────────────────────────────────

void send_data_via_ble() {
    // 示例：通过BLE发送数据到手机APP
    // 实际实现需要根据具体的BLE库
    
    static uint32_t last_ble_send = 0;
    uint32_t now = millis();
    
    // 每2秒发送一次数据
    if (now - last_ble_send > 2000) {
        // 获取系统状态数据
        // 打包为JSON格式
        // 通过BLE发送
        
        DEBUG_PRINTLN("[BLE] 发送数据到手机APP");
        last_ble_send = now;
    }
}

// ──────────────────────────────────────────────
// 系统状态监控
// ──────────────────────────────────────────────

void monitor_system_status() {
    // 示例：监控系统状态
    static uint32_t last_monitor_time = 0;
    uint32_t now = millis();
    
    if (now - last_monitor_time > 5000) {  // 每5秒监控一次
        // 获取调度器统计
        SchedulerStats stats = wrist_scheduler_get_stats();
        
        DEBUG_PRINTF("[监控] HR采样: %lu, 计算: %lu\n", stats.hr_samples, stats.hr_calcs);
