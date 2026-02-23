/*
 * 糖尿病初筛监测系统 - 主程序
 * 
 * 硬件：
 * - MAX30102: 心率/血氧传感器（I2C）
 * - SnO₂ + HKG-07: 气体传感器（模拟 + 数字触发）
 * - AD623: 低噪声运放 + RC滤波
 * - DHT20/AHT20: 环境温湿度（I2C，可选）
 * 
 * 功能：
 * - 分时采集多传感器数据
 * - 非阻塞定时（millis()）
 * - 状态机控制测量流程
 * - 输出风险初筛报告
 */

#include "scheduler.h"
#include "system_state.h"
#include "hr_algorithm.h"  // 包含HR_SUCCESS等常量定义

// ──────────────────────────────────────────────
// 状态机定义
enum SystemMode {
    MODE_IDLE,          // 待机
    MODE_MEASURING,     // 测量中
    MODE_REPORT         // 报告输出
};

// ──────────────────────────────────────────────
// 配置参数
#define IDLE_TIMEOUT_MS         5000    // 待机5秒后自动进入测量
#define MEASUREMENT_DURATION_MS 10000   // 测量窗口10秒
#define REPORT_DURATION_MS      2000    // 报告输出2秒

// ──────────────────────────────────────────────
// 全局变量
static SystemMode g_mode = MODE_IDLE;
static unsigned long g_mode_start_ms = 0;

// ──────────────────────────────────────────────
// 函数声明
void output_report();

// ──────────────────────────────────────────────
// setup()
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ;  // 等待串口就绪（最多3秒）
    }

    Serial.println(F("=== 糖尿病初筛监测系统启动 ==="));

    // 初始化调度器（包含所有传感器）
    scheduler_init();

    // 状态机初始化
    g_mode = MODE_IDLE;
    g_mode_start_ms = millis();

    Serial.println(F("系统就绪，等待测量..."));
}

// ──────────────────────────────────────────────
// loop()
void loop() {
    unsigned long now = millis();

    // 持续运行调度器（非阻塞，分时采集）
    scheduler_run();

    // ─── 状态机处理 ──────────────────────────────
    switch (g_mode) {
        case MODE_IDLE:
            // 待机：等待超时或外部触发
            if (now - g_mode_start_ms >= IDLE_TIMEOUT_MS) {
                g_mode = MODE_MEASURING;
                g_mode_start_ms = now;
                Serial.println(F("\n>>> 开始测量 <<<"));
                system_state_reset_measurement();  // 清空旧数据
            }
            break;

        case MODE_MEASURING:
            // 测量中：持续采集数据
            if (now - g_mode_start_ms >= MEASUREMENT_DURATION_MS) {
                g_mode = MODE_REPORT;
                g_mode_start_ms = now;
                Serial.println(F("\n>>> 测量完成，生成报告 <<<"));
            }
            break;

        case MODE_REPORT:
            // 报告输出
            output_report();
            delay(REPORT_DURATION_MS);  // 短暂延迟确保报告输出完成
            g_mode = MODE_IDLE;
            g_mode_start_ms = millis();
            Serial.println(F("\n>>> 返回待机 <<<\n"));
            break;
    }
}

// ──────────────────────────────────────────────
// 输出报告
void output_report() {
    const SystemState* s = system_state_get();

    Serial.println(F("\n─────────────────────────────────"));
    Serial.println(F("        测量报告"));
    Serial.println(F("─────────────────────────────────"));

    // 心率数据
    Serial.print(F("心率: "));
    if (s->hr_status == HR_SUCCESS && s->hr_bpm > 0) {
        Serial.print(s->hr_bpm, 1);
        Serial.print(F(" BPM (SNR: "));
        Serial.print(s->hr_snr_db, 1);
        Serial.println(F(" dB)"));
    } else {
        Serial.print(F("无效"));
        if (s->hr_status == HR_POOR_SIGNAL) {
            Serial.println(F(" - 信号质量差"));
        } else if (s->hr_status == HR_BUFFER_NOT_FULL) {
            Serial.println(F(" - 缓冲未满"));
        } else {
            Serial.println(F(" - 未检测到"));
        }
    }

    // 气体数据
    Serial.print(F("气体: "));
    if (s->gas_valid) {
        Serial.print(s->gas_concentration_ppm, 1);
        Serial.print(F(" ppm (电压: "));
        Serial.print(s->gas_voltage_mv, 1);
        Serial.println(F(" mV)"));
    } else {
        Serial.println(F("无效 - 预热中或无呼吸触发"));
    }

    // 环境数据
    Serial.print(F("环境: "));
    if (s->env_valid) {
        Serial.print(F("温度 "));
        Serial.print(s->env_temperature_c, 1);
        Serial.print(F("℃, 湿度 "));
        Serial.print(s->env_humidity_rh, 1);
        Serial.println(F("%"));
    } else {
        Serial.println(F("无效 - 传感器未就绪"));
    }

    Serial.println(F("─────────────────────────────────\n"));

    // TODO: 这里可以调用 risk_assessment 模块进行风险评估
    // assess_risk(s->hr_bpm, s->gas_concentration_ppm, ...);
}
