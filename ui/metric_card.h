/*
 * metric_card.h - 数据卡片组件头文件
 * 
 * 功能：显示心率、血氧、丙酮等医疗数据
 * 设计原则：
 * 1. 清晰的数值显示
 * 2. 颜色编码的状态指示
 * 3. 趋势箭头显示
 * 4. 单位标注
 */

#ifndef METRIC_CARD_H
#define METRIC_CARD_H

#include "ui_component.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 指标类型 ====================
typedef enum {
    METRIC_HEART_RATE = 0,      // 心率
    METRIC_BLOOD_OXYGEN,        // 血氧
    METRIC_ACETONE,             // 丙酮
    METRIC_TEMPERATURE,         // 体温
    METRIC_BLOOD_PRESSURE,      // 血压
    METRIC_RESPIRATORY_RATE,    // 呼吸率
    METRIC_GLUCOSE,             // 血糖
    METRIC_CUSTOM               // 自定义指标
} MetricType;

// ==================== 趋势方向 ====================
typedef enum {
    TREND_NONE = 0,     // 无趋势
    TREND_UP,           // 上升
    TREND_DOWN,         // 下降
    TREND_STABLE        // 稳定
} TrendDirection;

// ==================== 数值状态 ====================
typedef enum {
    VALUE_NORMAL = 0,   // 正常
    VALUE_WARNING,      // 警告
    VALUE_DANGER,       // 危险
    VALUE_UNKNOWN       // 未知
} ValueStatus;

// ==================== 卡片样式 ====================
typedef enum {
    CARD_STYLE_COMPACT = 0,     // 紧凑样式
    CARD_STYLE_STANDARD,        // 标准样式
    CARD_STYLE_DETAILED,        // 详细样式
    CARD_STYLE_HIGHLIGHT        // 高亮样式
} CardStyle;

// ==================== 卡片配置 ====================
typedef struct {
    MetricType type;            // 指标类型
    CardStyle style;            // 卡片样式
    bool show_title;            // 显示标题
    bool show_unit;             // 显示单位
    bool show_trend;            // 显示趋势
    bool show_status_icon;      // 显示状态图标
    bool show_last_update;      // 显示最后更新时间
    bool show_confidence;       // 显示可信度
    uint8_t update_interval_ms; // 更新间隔
} MetricCardConfig;

// ==================== 卡片数据 ====================
typedef struct {
    // 基本数据
    float value;                // 数值
    float min_value;            // 最小值
    float max_value;            // 最大值
    float reference_value;      // 参考值
    
    // 状态信息
    ValueStatus status;         // 数值状态
    TrendDirection trend;       // 趋势方向
    uint8_t confidence;         // 可信度 0-100%
    
    // 时间信息
    uint32_t timestamp;         // 时间戳
    uint32_t measurement_time;  // 测量时间（秒）
    
    // 文本信息
    const char* title;          // 标题
    const char* unit;           // 单位
    const char* status_text;    // 状态文本
} MetricCardData;

// ==================== 卡片创建 ====================
UiComponent* metric_card_create(const char* name, const Rect* bounds, MetricType type);

// ==================== 配置管理 ====================
void metric_card_set_config(UiComponent* card, const MetricCardConfig* config);
void metric_card_get_config(const UiComponent* card, MetricCardConfig* config);

// ==================== 数据更新 ====================
void metric_card_set_value(UiComponent* card, float value);
void metric_card_set_range(UiComponent* card, float min_value, float max_value);
void metric_card_set_reference(UiComponent* card, float reference_value);
void metric_card_set_status(UiComponent* card, ValueStatus status);
void metric_card_set_trend(UiComponent* card, TrendDirection trend);
void metric_card_set_confidence(UiComponent* card, uint8_t confidence);
void metric_card_set_timestamp(UiComponent* card, uint32_t timestamp);
void metric_card_set_measurement_time(UiComponent* card, uint32_t seconds);

// ==================== 批量更新 ====================
void metric_card_update_all(UiComponent* card, const MetricCardData* data);

// ==================== 文本设置 ====================
void metric_card_set_title(UiComponent* card, const char* title);
void metric_card_set_unit(UiComponent* card, const char* unit);
void metric_card_set_status_text(UiComponent* card, const char* text);

// ==================== 样式设置 ====================
void metric_card_set_style(UiComponent* card, CardStyle style);
void metric_card_set_text_color(UiComponent* card, uint16_t color);
void metric_card_set_value_color(UiComponent* card, uint16_t color);
void metric_card_set_background_color(UiComponent* card, uint16_t color);
void metric_card_set_border_color(UiComponent* card, uint16_t color);
void metric_card_set_font_size(UiComponent* card, uint8_t title_size, uint8_t value_size);

// ==================== 可见性控制 ====================
void metric_card_show_title(UiComponent* card, bool show);
void metric_card_show_unit(UiComponent* card, bool show);
void metric_card_show_trend(UiComponent* card, bool show);
void metric_card_show_status_icon(UiComponent* card, bool show);
void metric_card_show_last_update(UiComponent* card, bool show);
void metric_card_show_confidence(UiComponent* card, bool show);

// ==================== 动画效果 ====================
void metric_card_start_pulse(UiComponent* card);
void metric_card_stop_pulse(UiComponent* card);
void metric_card_start_blink(UiComponent* card, uint16_t interval_ms);
void metric_card_stop_blink(UiComponent* card);
void metric_card_animate_value_change(UiComponent* card, float new_value, uint32_t duration_ms);

// ==================== 工具函数 ====================
uint16_t metric_card_get_preferred_width(MetricType type, CardStyle style);
uint16_t metric_card_get_preferred_height(MetricType type, CardStyle style);
void metric_card_auto_adjust_size(UiComponent* card);

// ==================== 状态判断 ====================
ValueStatus metric_card_calculate_status(float value, float min_normal, float max_normal);
TrendDirection metric_card_calculate_trend(float current, float previous);
uint16_t metric_card_get_status_color(ValueStatus status);
const char* metric_card_get_status_icon(ValueStatus status);
const char* metric_card_get_trend_icon(TrendDirection trend);

// ==================== 格式化函数 ====================
void metric_card_format_value(char* buffer, size_t buffer_size, 
                             float value, MetricType type);
void metric_card_format_unit(char* buffer, size_t buffer_size, MetricType type);
void metric_card_format_time_ago(char* buffer, size_t buffer_size, 
                                uint32_t timestamp);

// ==================== 默认配置 ====================
extern const MetricCardConfig METRIC_CARD_DEFAULT_CONFIG;

// ==================== 预定义卡片类型 ====================
// 心率卡片
UiComponent* metric_card_create_heart_rate(const char* name, const Rect* bounds);
// 血氧卡片
UiComponent* metric_card_create_blood_oxygen(const char* name, const Rect* bounds);
// 丙酮卡片
UiComponent* metric_card_create_acetone(const char* name, const Rect* bounds);
// 体温卡片
UiComponent* metric_card_create_temperature(const char* name, const Rect* bounds);

// ==================== 医疗阈值 ====================
typedef struct {
    float normal_min;
    float normal_max;
    float warning_min;
    float warning_max;
    float danger_min;
    float danger_max;
} MedicalThresholds;

extern const MedicalThresholds HEART_RATE_THRESHOLDS;
extern const MedicalThresholds BLOOD_OXYGEN_THRESHOLDS;
extern const MedicalThresholds ACETONE_THRESHOLDS;
extern const MedicalThresholds TEMPERATURE_THRESHOLDS;

// ==================== 获取医疗阈值 ====================
const MedicalThresholds* metric_card_get_thresholds(MetricType type);

#ifdef __cplusplus
}
#endif

#endif // METRIC_CARD_H