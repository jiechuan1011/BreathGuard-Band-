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
