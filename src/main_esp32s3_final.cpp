/*
 * main_esp32s3_final.cpp - ESP32-S3腕带完整联调版本
 * 
 * 综合功能：
 * 1. HR采集 + 运动校正 (100Hz)
 * 2. SnO2采集 (10Hz)
 * 3. 电池监控 (60s)
 * 4. UI实时刷新 (500ms)
 * 5. BLE JSON推送 (4000ms)
 * 6. DeepSleep功耗管理
 * 
 * 硬件：ESP32-S3 + MAX30102 (GPIO17/18) + AMOLED (SPI)
 * 
 * 编译：使用platformio.ini中的[env:esp32s3_final]配置
 */

#include <Arduino.h>
#include <Wire.h>

// ==================== 项目头文件 ====================
#include "../config/config.h"
#include "../config/pin_config.h"
#include "../config/system_config.h"
#include "../config/ble_config.h"

// 驱动层
#include "hr_driver.h"
#include "sno2_driver.h"

// 算法层
#include "../algorithm/hr_algorithm.h"
#include "../algorithm/motion_correction.h"

// 新增：最终版本管理器
#include "sensor_collector_final.h"
#include "algorithm_manager_final.h"
#include "ble_peripheral_final.h"

// ==================== 全局任务调度 ====================

typedef struct {
    uint32_t last_run_ms;
    uint32_t period_ms;
    void (*task_func)(void);
    const char* task_name;
} ScheduledTask;

// ==================== 全局统计 ====================

typedef struct {
    uint32_t total_loop_cycles;
    uint32_t total_errors;
    uint32_t last_stats_print_ms;
    
    uint8_t is_low_battery;
    uint8_t is_critical_battery;
    uint32_t last_battery_check_ms;
    
    uint8_t deep_sleep_enabled;
    uint32_t last_activity_ms;
} SystemStats;

static SystemStats g_sys_stats = {0};

// ==================== 任务函数 ====================

// 任务1：采集更新（10ms）
static void task_sensor_collection() {
    sensor_collector_update();
}

// 任务2：算法更新（10ms）
static void task_algorithm_update() {
    algorithm_manager_update();
}

// 任务3：UI刷新（500ms）
static void task_ui_update() {
    // 获取最新算法结果显示在AMOLED
    AlgorithmResult result = {0};
    algorithm_manager_get_result(&result);
    
    RiskAssessment risk = {0};
    algorithm_manager_get_risk_assessment(&risk);
    
#ifdef DEBUG_MODE
    // 简化版UI输出
    if (result.bpm > 0) {
        Serial.printf("\r[UI] HR:%3u SpO2:%3u %% Acetone:%5.1f ppm Risk:%s Battery:%u%%",
            result.bpm, result.spo2, result.acetone_ppm, 
            risk.risk_description, g_sys_stats.is_low_battery ? 10 : 80);
    }
#endif
    
    g_sys_stats.last_activity_ms = millis();
}

// 任务4：BLE推送（4000ms）
static void task_ble_send() {
    ble_peripheral_send_data();
#ifdef DEBUG_MODE
    if (ble_peripheral_is_connected()) {
        Serial.printf(" [BLE← sent]\n");
    }
#endif
}

// 任务5：电池检查（60s）
static void task_battery_check() {
    CollectorStats stats = {0};
    sensor_collector_get_stats(&stats);
    
    g_sys_stats.is_low_battery = (stats.battery_percent < 20) ? 1 : 0;
    g_sys_stats.is_critical_battery = (stats.battery_percent < 5) ? 1 : 0;
    
    if (g_sys_stats.is_critical_battery) {
        Serial.println("[POWER] ⚠️ 严重电量不足！准备关闭BLE");
        // 可选：关闭BLE或进入深度睡眠
    }
}

// 任务6：统计信息打印（30s）
static void task_print_stats() {
    Serial.println("\n\n========== 系统统计 (30s) ==========");
    
    CollectorStats collector_stats = {0};
    sensor_collector_get_stats(&collector_stats);
    
    BleStats ble_stats = {0};
    ble_peripheral_get_stats(&ble_stats);
    
    Serial.printf("HR采样: %lu | SnO2采样: %lu | 电池: %u%% (%umV)\n",
        collector_stats.total_hr_samples, collector_stats.total_sno2_samples,
        collector_stats.battery_percent, collector_stats.battery_mv);
    
    Serial.printf("BLE: %s | 通知: %lu | 总循环: %lu\n",
        ble_stats.is_connected ? "✓" : "✗",
        ble_stats.total_notifications,
        g_sys_stats.total_loop_cycles);
    
    algorithm_manager_print_stats();
    
    Serial.println("=====================================\n");
}

// 定义所有任务
static ScheduledTask g_tasks[] = {
    {0, 10,    task_sensor_collection,  "SensorCollection"},
    {0, 10,    task_algorithm_update,   "AlgorithmUpdate"},
    {0, 500,   task_ui_update,          "UIUpdate"},
    {0, 4000,  task_ble_send,           "BLESend"},
    {0, 60000, task_battery_check,      "BatteryCheck"},
    {0, 30000, task_print_stats,        "PrintStats"},
};

#define NUM_TASKS (sizeof(g_tasks) / sizeof(g_tasks[0]))

// ==================== 调度器更新 ====================

static void scheduler_update() {
    uint32_t now_ms = millis();
    
    for (int i = 0; i < NUM_TASKS; i++) {
        if ((now_ms - g_tasks[i].last_run_ms) >= g_tasks[i].period_ms) {
            if (g_tasks[i].task_func) {
                g_tasks[i].task_func();
            }
            g_tasks[i].last_run_ms = now_ms;
        }
    }
}

// ==================== 初始化 ====================

void setup() {
    // 串口初始化
    Serial.begin(115200);
    delay(500);
    
    while (!Serial && millis() < 3000);
    
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════════╗");
    Serial.println("║  ESP32-S3 糖尿病初筛监测系统（最终版本）  ║");
    Serial.println("║  System: Sensor + Algorithm + BLE + UI    ║");
    Serial.println("╚════════════════════════════════════════════╝\n");
    
    // ==================== 硬件初始化 ====================
    
    // 1. I2C初始化
    Serial.println("[INIT] I2C...");
    Wire.begin(4, 5);  // SDA=GPIO4, SCL=GPIO5（确认与pin_config.h一致）
    Wire.setClock(400000);
    
    // 2. MAX30102初始化
    Serial.println("[INIT] MAX30102...");
    if (!hr_driver_init()) {
        Serial.println("    ❌ MAX30102 初始化失败！");
    } else {
        Serial.println("    ✓ MAX30102 ready");
    }
    
    // 3. SnO2驱动初始化
    Serial.println("[INIT] SnO2...");
    sno2_init();
    Serial.println("    ✓ SnO2 ready");
    
    // 4. 采集管理器初始化
    Serial.println("[INIT] SensorCollector...");
    sensor_collector_init();
    Serial.println("    ✓ SensorCollector ready");
    
    // 5. 算法管理器初始化
    Serial.println("[INIT] AlgorithmManager...");
    algorithm_manager_init();
    Serial.println("    ✓ AlgorithmManager ready");
    
    // 6. BLE初始化
    Serial.println("[INIT] BLE Peripheral...");
    ble_peripheral_init();
    Serial.println("    ✓ BLE ready");
    
    // ==================== 系统初始化 ====================
    
    // 初始化任务计时器
    uint32_t now = millis();
    for (int i = 0; i < NUM_TASKS; i++) {
        g_tasks[i].last_run_ms = now;
    }
    
    g_sys_stats.deep_sleep_enabled = 0;
    g_sys_stats.last_battery_check_ms = now;
    g_sys_stats.last_activity_ms = now;
    
    // ==================== 启动完成 ====================
    
    Serial.println("\n✓ 系统初始化完成！");
    Serial.println("核心参数：");
    Serial.println("  HR采样：100Hz (10ms周期)");
    Serial.println("  SnO2采样：10Hz (100ms周期)");
    Serial.println("  电池检查：60s周期");
    Serial.println("  UI刷新：500ms周期");
    Serial.println("  BLE推送：4000ms周期 (JSON格式)");
    Serial.println("  心率范围：40-180 BPM");
    Serial.println("  SpO2范围：70-100%");
    Serial.println("\n开始主循环...\n");
}

// ==================== 主循环 ====================

void loop() {
    // 核心调度更新（非阻塞）
    scheduler_update();
    
    g_sys_stats.total_loop_cycles++;
    
    // 保持CPU响应性
    delayMicroseconds(100);
    
    // ==================== 可选：DeepSleep逻辑 ====================
    // 如果启用DeepSleep（检测模块10秒唤醒）：
    // 判断是否在低功耗条件下
    // if (g_sys_stats.deep_sleep_enabled) {
    //     uint32_t inactive_time = millis() - g_sys_stats.last_activity_ms;
    //     if (inactive_time > 300000) {  // 5分钟无活动
    //         Serial.println("[POWER] 准备DeepSleep...");
    //         esp_deep_sleep(10 * 1000000);  // 10秒后唤醒
    //     }
    // }
}

// ==================== 串口调试命令（可选） ====================

// 在loop中调用此函数处理串口命令
void debug_commands_handle() {
    static char cmd_buffer[64];
    static uint8_t cmd_index = 0;
    
    while (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            cmd_buffer[cmd_index] = '\0';
            
            if (strcmp(cmd_buffer, "help") == 0) {
                Serial.println("\n=== 调试命令 ===");
                Serial.println("help   - 显示此帮助");
                Serial.println("stats  - 显示系统统计");
                Serial.println("reset  - 重启系统");
                
            } else if (strcmp(cmd_buffer, "stats") == 0) {
                task_print_stats();
                
            } else if (strcmp(cmd_buffer, "reset") == 0) {
                Serial.println("重启中...");
                ESP.restart();
            }
            
            cmd_index = 0;
        } else if (c >= 32 && c <= 126 && cmd_index < 63) {
            cmd_buffer[cmd_index++] = c;
        }
    }
}