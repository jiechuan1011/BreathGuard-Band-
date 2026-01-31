/*
 * waveform_view.h - 波形视图组件头文件
 * 
 * 功能：显示心电图、呼吸波形、实时数据流
 * 设计原则：
 * 1. 实时波形显示
 * 2. 平滑滚动效果
 * 3. 多通道支持
 * 4. 医疗级精度
 */

#ifndef WAVEFORM_VIEW_H
#define WAVEFORM_VIEW_H

#include "ui_component.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 波形类型 ====================
typedef enum {
    WAVEFORM_ECG = 0,           // 心电图
    WAVEFORM_PPG,               // 光电容积图
    WAVEFORM_RESPIRATION,       // 呼吸波形
    WAVEFORM_ACETONE,           // 丙酮浓度
    WAVEFORM_CUSTOM             // 自定义波形
} WaveformType;

// ==================== 显示模式 ====================
typedef enum {
    DISPLAY_MODE_REALTIME = 0,  // 实时模式
    DISPLAY_MODE_SCROLL,        // 滚动模式
    DISPLAY_MODE_FROZEN,        // 冻结模式
    DISPLAY_MODE_AVERAGE,       // 平均模式
    DISPLAY_MODE_TREND          // 趋势模式
} DisplayMode;

// ==================== 网格样式 ====================
typedef enum {
    GRID_NONE = 0,              // 无网格
    GRID_LIGHT,                 // 浅网格
    GRID_MEDIUM,                // 中等网格
    GRID_DENSE,                 // 密集网格
    GRID_MEDICAL                // 医疗网格
} GridStyle;

// ==================== 波形样式 ====================
typedef enum {
    WAVE_STYLE_LINE = 0,        // 线条
    WAVE_STYLE_FILLED,          // 填充
    WAVE_STYLE_DOTTED,          // 点线
    WAVE_STYLE_BAR,             // 柱状
    WAVE_STYLE_SPLINE           // 样条曲线
} WaveStyle;

// ==================== 波形配置 ====================
typedef struct {
    WaveformType type;          // 波形类型
    DisplayMode display_mode;   // 显示模式
    GridStyle grid_style;       // 网格样式
    WaveStyle wave_style;       // 波形样式
    
    // 尺寸
    uint16_t width;             // 宽度
    uint16_t height;            // 高度
    uint8_t channels;           // 通道数
    
    // 时间轴
    uint32_t time_window_ms;    // 时间窗口（毫秒）
    uint32_t sample_rate_hz;    // 采样率（Hz）
    uint16_t points_per_screen; // 每屏点数
    
    // 幅度
    float amplitude_min;        // 最小幅度
    float amplitude_max;        // 最大幅度
    float amplitude_scale;      // 幅度缩放
    
    // 颜色
    uint16_t grid_color;        // 网格颜色
    uint16_t wave_color;        // 波形颜色
    uint16_t background_color;  // 背景颜色
    uint16_t axis_color;        // 坐标轴颜色
    uint16_t marker_color;      // 标记颜色
    
    // 显示选项
    bool show_grid;             // 显示网格
    bool show_axis;             // 显示坐标轴
    bool show_markers;          // 显示标记
    bool show_values;           // 显示数值
    bool show_heartbeat;        // 显示心跳标记
    bool show_breathing;        // 显示呼吸标记
    
    // 性能
    uint8_t update_fps;         // 更新帧率
    bool use_double_buffer;     // 使用双缓冲
} WaveformConfig;

// ==================== 波形数据点 ====================
typedef struct {
    float value;                // 数值
    uint32_t timestamp;         // 时间戳
    uint8_t channel;            // 通道号
    uint8_t flags;              // 标志位
} WaveformPoint;

// ==================== 波形数据缓冲区 ====================
typedef struct {
    WaveformPoint* buffer;      // 数据缓冲区
    uint16_t capacity;          // 缓冲区容量
    uint16_t head;              // 头部索引
    uint16_t tail;              // 尾部索引
    uint16_t count;             // 数据点数
    bool is_full;               // 是否已满
} WaveformBuffer;

// ==================== 波形标记 ====================
typedef struct {
    uint32_t timestamp;         // 时间戳
    float value;                // 数值
    const char* label;          // 标签
    uint16_t color;             // 颜色
    uint8_t type;               // 类型
} WaveformMarker;

// ==================== 波形视图创建 ====================
UiComponent* waveform_view_create(const char* name, const Rect* bounds, WaveformType type);

// ==================== 配置管理 ====================
void waveform_view_set_config(UiComponent* view, const WaveformConfig* config);
void waveform_view_get_config(const UiComponent* view, WaveformConfig* config);

// ==================== 数据管理 ====================
bool waveform_view_add_point(UiComponent* view, float value, uint8_t channel);
bool waveform_view_add_point_with_timestamp(UiComponent* view, float value, uint32_t timestamp, uint8_t channel);
void waveform_view_clear_data(UiComponent* view);
void waveform_view_set_data(UiComponent* view, const float* data, uint16_t count, uint8_t channel);

// ==================== 缓冲区管理 ====================
bool waveform_view_init_buffer(UiComponent* view, uint16_t capacity);
void waveform_view_free_buffer(UiComponent* view);
uint16_t waveform_view_get_data_count(UiComponent* view, uint8_t channel);
bool waveform_view_is_buffer_full(UiComponent* view);

// ==================== 显示控制 ====================
void waveform_view_set_display_mode(UiComponent* view, DisplayMode mode);
void waveform_view_set_grid_style(UiComponent* view, GridStyle style);
void waveform_view_set_wave_style(UiComponent* view, WaveStyle style);
void waveform_view_set_time_window(UiComponent* view, uint32_t window_ms);
void waveform_view_set_sample_rate(UiComponent* view, uint32_t rate_hz);

// ==================== 幅度控制 ====================
void waveform_view_set_amplitude_range(UiComponent* view, float min, float max);
void waveform_view_set_amplitude_scale(UiComponent* view, float scale);
void waveform_view_auto_scale(UiComponent* view);
void waveform_view_fit_to_data(UiComponent* view);

// ==================== 颜色设置 ====================
void waveform_view_set_grid_color(UiComponent* view, uint16_t color);
void waveform_view_set_wave_color(UiComponent* view, uint16_t color);
void waveform_view_set_background_color(UiComponent* view, uint16_t color);
void waveform_view_set_axis_color(UiComponent* view, uint16_t color);
void waveform_view_set_channel_color(UiComponent* view, uint8_t channel, uint16_t color);

// ==================== 标记管理 ====================
void waveform_view_add_marker(UiComponent* view, uint32_t timestamp, float value, 
                             const char* label, uint16_t color);
void waveform_view_clear_markers(UiComponent* view);
void waveform_view_set_heartbeat_marker(UiComponent* view, uint32_t timestamp);
void waveform_view_set_breathing_marker(UiComponent* view, uint32_t timestamp);

// ==================== 可见性控制 ====================
void waveform_view_show_grid(UiComponent* view, bool show);
void waveform_view_show_axis(UiComponent* view, bool show);
void waveform_view_show_markers(UiComponent* view, bool show);
void waveform_view_show_values(UiComponent* view, bool show);
void waveform_view_show_heartbeat(UiComponent* view, bool show);
void waveform_view_show_breathing(UiComponent* view, bool show);

// ==================== 动画控制 ====================
void waveform_view_start_scroll(UiComponent* view, uint32_t speed_ms_per_pixel);
void waveform_view_stop_scroll(UiComponent* view);
void waveform_view_pause(UiComponent* view);
void waveform_view_resume(UiComponent* view);
void waveform_view_freeze(UiComponent* view);
void waveform_view_unfreeze(UiComponent* view);

// ==================== 性能优化 ====================
void waveform_view_set_update_fps(UiComponent* view, uint8_t fps);
void waveform_view_enable_double_buffer(UiComponent* view, bool enable);
void waveform_view_set_rendering_quality(UiComponent* view, uint8_t quality);

// ==================== 医疗特性 ====================
void waveform_view_detect_heartbeat(UiComponent* view);
void waveform_view_detect_respiration(UiComponent* view);
void waveform_view_calculate_hrv(UiComponent* view, float* hrv);
void waveform_view_calculate_rr_interval(UiComponent* view, float* rr_ms);

// ==================== 工具函数 ====================
float waveform_view_get_min_value(UiComponent* view, uint8_t channel);
float waveform_view_get_max_value(UiComponent* view, uint8_t channel);
float waveform_view_get_average_value(UiComponent* view, uint8_t channel);
uint32_t waveform_view_get_duration(UiComponent* view);
uint16_t waveform_view_get_points_on_screen(UiComponent* view);

// ==================== 坐标转换 ====================
int16_t waveform_view_value_to_y(UiComponent* view, float value, uint8_t channel);
int16_t waveform_view_time_to_x(UiComponent* view, uint32_t timestamp);
float waveform_view_y_to_value(UiComponent* view, int16_t y, uint8_t channel);
uint32_t waveform_view_x_to_time(UiComponent* view, int16_t x);

// ==================== 默认配置 ====================
extern const WaveformConfig WAVEFORM_DEFAULT_CONFIG;

// ==================== 预定义波形类型 ====================
// 心电图视图
UiComponent* waveform_view_create_ecg(const char* name, const Rect* bounds);
// 血氧波形视图
UiComponent* waveform_view_create_ppg(const char* name, const Rect* bounds);
// 呼吸波形视图
UiComponent* waveform_view_create_respiration(const char* name, const Rect* bounds);
// 丙酮浓度视图
UiComponent* waveform_view_create_acetone(const char* name, const Rect* bounds);

// ==================== 医疗分析函数 ====================
typedef struct {
    uint32_t heartbeat_count;   // 心跳次数
    float heart_rate;           // 心率
    float hrv;                  // 心率变异性
    float respiration_rate;     // 呼吸率
    float signal_quality;       // 信号质量
    uint8_t arrhythmia_flag;    // 心律失常标志
} WaveformAnalysis;

bool waveform_view_analyze_ecg(UiComponent* view, WaveformAnalysis* analysis);
bool waveform_view_analyze_ppg(UiComponent* view, WaveformAnalysis* analysis);
bool waveform_view_analyze_respiration(UiComponent* view, WaveformAnalysis* analysis);

// ==================== 事件回调 ====================
typedef void (*WaveformCallback)(UiComponent* view, uint8_t event_type, void* user_data);

void waveform_view_set_heartbeat_callback(UiComponent* view, WaveformCallback callback, void* user_data);
void waveform_view_set_respiration_callback(UiComponent* view, WaveformCallback callback, void* user_data);
void waveform_view_set_data_ready_callback(UiComponent* view, WaveformCallback callback, void* user_data);
void waveform_view_set_analysis_complete_callback(UiComponent* view, WaveformCallback callback, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // WAVEFORM_VIEW_H