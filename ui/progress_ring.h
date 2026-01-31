/*
 * progress_ring.h - 环形进度条组件头文件
 * 
 * 功能：显示测试进度、倒计时、完成状态
 * 设计原则：
 * 1. 平滑的动画效果
 * 2. 多状态显示
 * 3. 中心文本显示
 * 4. 颜色渐变
 */

#ifndef PROGRESS_RING_H
#define PROGRESS_RING_H

#include "ui_component.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 进度环模式 ====================
typedef enum {
    RING_MODE_PROGRESS = 0,     // 进度模式
    RING_MODE_COUNTDOWN,        // 倒计时模式
    RING_MODE_LOADING,          // 加载模式
    RING_MODE_COMPLETE,         // 完成模式
    RING_MODE_ERROR,            // 错误模式
    RING_MODE_PAUSED            // 暂停模式
} RingMode;

// ==================== 环样式 ====================
typedef enum {
    RING_STYLE_SOLID = 0,       // 实心环
    RING_STYLE_DASHED,          // 虚线环
    RING_STYLE_GRADIENT,        // 渐变环
    RING_STYLE_MULTI_COLOR      // 多色环
} RingStyle;

// ==================== 文本位置 ====================
typedef enum {
    TEXT_CENTER = 0,            // 中心
    TEXT_INSIDE,                // 环内
    TEXT_OUTSIDE,               // 环外
    TEXT_HIDDEN                 // 隐藏
} TextPosition;

// ==================== 动画类型 ====================
typedef enum {
    RING_ANIMATION_NONE = 0,    // 无动画
    RING_ANIMATION_LINEAR,      // 线性动画
    RING_ANIMATION_EASE_IN,     // 缓入动画
    RING_ANIMATION_EASE_OUT,    // 缓出动画
    RING_ANIMATION_EASE_IN_OUT, // 缓入缓出
    RING_ANIMATION_BOUNCE       // 弹跳动画
} RingAnimation;

// ==================== 进度环配置 ====================
typedef struct {
    RingMode mode;              // 环模式
    RingStyle style;            // 环样式
    RingAnimation animation;    // 动画类型
    TextPosition text_position; // 文本位置
    
    // 尺寸
    uint16_t outer_radius;      // 外半径
    uint16_t inner_radius;      // 内半径
    uint16_t thickness;         // 环厚度
    
    // 颜色
    uint16_t ring_color;        // 环颜色
    uint16_t background_color;  // 背景颜色
    uint16_t progress_color;    // 进度颜色
    uint16_t complete_color;    // 完成颜色
    uint16_t error_color;       // 错误颜色
    
    // 文本
    bool show_text;             // 显示文本
    bool show_percentage;       // 显示百分比
    bool show_time;             // 显示时间
    bool show_status;           // 显示状态
    
    // 动画
    uint32_t animation_duration_ms; // 动画持续时间
    uint8_t animation_fps;      // 动画帧率
} ProgressRingConfig;

// ==================== 进度环数据 ====================
typedef struct {
    // 进度值
    float progress;             // 进度 0.0-1.0
    float target_progress;      // 目标进度
    
    // 时间相关
    uint32_t total_time_ms;     // 总时间（毫秒）
    uint32_t elapsed_time_ms;   // 已过时间（毫秒）
    uint32_t remaining_time_ms; // 剩余时间（毫秒）
    
    // 文本
    const char* primary_text;   // 主要文本
    const char* secondary_text; // 次要文本
    const char* status_text;    // 状态文本
    
    // 状态
    bool is_complete;           // 是否完成
    bool has_error;             // 是否有错误
    bool is_paused;             // 是否暂停
} ProgressRingData;

// ==================== 进度环创建 ====================
UiComponent* progress_ring_create(const char* name, const Rect* bounds);

// ==================== 配置管理 ====================
void progress_ring_set_config(UiComponent* ring, const ProgressRingConfig* config);
void progress_ring_get_config(const UiComponent* ring, ProgressRingConfig* config);

// ==================== 进度控制 ====================
void progress_ring_set_progress(UiComponent* ring, float progress);
void progress_ring_set_target_progress(UiComponent* ring, float target);
void progress_ring_increment_progress(UiComponent* ring, float increment);
void progress_ring_reset_progress(UiComponent* ring);

// ==================== 时间控制 ====================
void progress_ring_set_total_time(UiComponent* ring, uint32_t total_ms);
void progress_ring_set_elapsed_time(UiComponent* ring, uint32_t elapsed_ms);
void progress_ring_update_time(UiComponent* ring, uint32_t delta_ms);
void progress_ring_start_countdown(UiComponent* ring, uint32_t duration_ms);
void progress_ring_pause_countdown(UiComponent* ring);
void progress_ring_resume_countdown(UiComponent* ring);
void progress_ring_stop_countdown(UiComponent* ring);

// ==================== 模式控制 ====================
void progress_ring_set_mode(UiComponent* ring, RingMode mode);
void progress_ring_set_style(UiComponent* ring, RingStyle style);
void progress_ring_set_animation(UiComponent* ring, RingAnimation animation);

// ==================== 文本设置 ====================
void progress_ring_set_primary_text(UiComponent* ring, const char* text);
void progress_ring_set_secondary_text(UiComponent* ring, const char* text);
void progress_ring_set_status_text(UiComponent* ring, const char* text);
void progress_ring_set_text_position(UiComponent* ring, TextPosition position);

// ==================== 颜色设置 ====================
void progress_ring_set_ring_color(UiComponent* ring, uint16_t color);
void progress_ring_set_background_color(UiComponent* ring, uint16_t color);
void progress_ring_set_progress_color(UiComponent* ring, uint16_t color);
void progress_ring_set_complete_color(UiComponent* ring, uint16_t color);
void progress_ring_set_error_color(UiComponent* ring, uint16_t color);
void progress_ring_set_gradient_colors(UiComponent* ring, 
                                      uint16_t start_color, 
                                      uint16_t end_color);

// ==================== 尺寸设置 ====================
void progress_ring_set_radius(UiComponent* ring, uint16_t outer_radius, uint16_t inner_radius);
void progress_ring_set_thickness(UiComponent* ring, uint16_t thickness);
void progress_ring_auto_adjust_size(UiComponent* ring);

// ==================== 可见性控制 ====================
void progress_ring_show_text(UiComponent* ring, bool show);
void progress_ring_show_percentage(UiComponent* ring, bool show);
void progress_ring_show_time(UiComponent* ring, bool show);
void progress_ring_show_status(UiComponent* ring, bool show);

// ==================== 状态控制 ====================
void progress_ring_set_complete(UiComponent* ring, bool complete);
void progress_ring_set_error(UiComponent* ring, bool error, const char* error_message);
void progress_ring_set_paused(UiComponent* ring, bool paused);
void progress_ring_set_loading(UiComponent* ring, bool loading);

// ==================== 动画控制 ====================
void progress_ring_start_animation(UiComponent* ring);
void progress_ring_stop_animation(UiComponent* ring);
void progress_ring_pause_animation(UiComponent* ring);
void progress_ring_resume_animation(UiComponent* ring);
void progress_ring_set_animation_duration(UiComponent* ring, uint32_t duration_ms);
void progress_ring_set_animation_fps(UiComponent* ring, uint8_t fps);

// ==================== 批量更新 ====================
void progress_ring_update_all(UiComponent* ring, const ProgressRingData* data);

// ==================== 工具函数 ====================
uint16_t progress_ring_get_preferred_size(uint16_t thickness);
float progress_ring_calculate_progress(uint32_t elapsed, uint32_t total);
uint32_t progress_ring_calculate_remaining_time(float progress, uint32_t total);
void progress_ring_format_time(char* buffer, size_t buffer_size, uint32_t milliseconds);
void progress_ring_format_percentage(char* buffer, size_t buffer_size, float progress);

// ==================== 动画辅助函数 ====================
float progress_ring_calculate_animation_progress(uint32_t start_time, uint32_t duration);
float progress_ring_apply_easing(float progress, RingAnimation animation);

// ==================== 默认配置 ====================
extern const ProgressRingConfig PROGRESS_RING_DEFAULT_CONFIG;

// ==================== 预定义环类型 ====================
// 丙酮检测环
UiComponent* progress_ring_create_acetone_test(const char* name, const Rect* bounds);
// 心率检测环
UiComponent* progress_ring_create_heart_rate_test(const char* name, const Rect* bounds);
// 综合检测环
UiComponent* progress_ring_create_comprehensive_test(const char* name, const Rect* bounds);
// 加载环
UiComponent* progress_ring_create_loading(const char* name, const Rect* bounds);

// ==================== 测试相关函数 ====================
void progress_ring_start_test(UiComponent* ring, uint32_t duration_ms, const char* test_name);
void progress_ring_update_test_progress(UiComponent* ring, uint32_t elapsed_ms);
void progress_ring_complete_test(UiComponent* ring, bool success, const char* result_text);
void progress_ring_cancel_test(UiComponent* ring);

// ==================== 事件回调 ====================
typedef void (*ProgressRingCallback)(UiComponent* ring, uint8_t event_type, void* user_data);

void progress_ring_set_complete_callback(UiComponent* ring, ProgressRingCallback callback, void* user_data);
void progress_ring_set_error_callback(UiComponent* ring, ProgressRingCallback callback, void* user_data);
void progress_ring_set_timeout_callback(UiComponent* ring, ProgressRingCallback callback, void* user_data);
void progress_ring_set_progress_callback(UiComponent* ring, ProgressRingCallback callback, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // PROGRESS_RING_H