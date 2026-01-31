/*
 * main_screen.h - 主界面头文件
 * 
 * 功能：糖尿病筛查手表主界面
 * 设计原则：
 * 1. 简洁明了的主界面
 * 2. 清晰的导航提示
 * 3. 实时状态显示
 * 4. 医疗数据概览
 */

#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include "ui_component.h"
#include "status_bar.h"
#include "metric_card.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 主界面布局 ====================
typedef struct {
    Rect status_bar_area;       // 状态栏区域
    Rect date_time_area;        // 日期时间区域
    Rect navigation_area;       // 导航提示区域
    Rect status_area;           // 状态显示区域
    Rect data_cards_area;       // 数据卡片区域
    Rect quick_actions_area;    // 快捷操作区域
} MainScreenLayout;

// ==================== 主界面数据 ====================
typedef struct {
    // 时间信息
    struct tm current_time;     // 当前时间
    const char* location;       // 位置
    const char* weather;        // 天气
    float temperature;          // 温度
    
    // 系统状态
    uint8_t battery_level;      // 电池电量
    bool bluetooth_connected;   // 蓝牙连接状态
    bool has_notifications;     // 是否有通知
    
    // 医疗状态
    const char* health_status;  // 健康状态
    TrendLevel overall_trend;   // 整体趋势
    uint32_t last_test_time;    // 上次测试时间
    
    // 导航提示
    const char* swipe_up_hint;  // 上滑提示
    const char* swipe_down_hint;// 下滑提示
    const char* button1_hint;   // 按键1提示
    const char* button2_hint;   // 按键2提示
    
    // 快速数据
    uint8_t last_heart_rate;    // 上次心率
    uint8_t last_blood_oxygen;  // 上次血氧
    float last_acetone_level;   // 上次丙酮浓度
} MainScreenData;

// ==================== 主界面配置 ====================
typedef struct {
    // 布局配置
    bool show_status_bar;       // 显示状态栏
    bool show_date_time;        // 显示日期时间
    bool show_navigation_hints; // 显示导航提示
    bool show_health_status;    // 显示健康状态
    bool show_quick_data;       // 显示快速数据
    bool show_quick_actions;    // 显示快捷操作
    
    // 样式配置
    uint16_t background_color;  // 背景颜色
    uint16_t text_color;        // 文字颜色
    uint16_t highlight_color;   // 高亮颜色
    uint8_t animation_speed;    // 动画速度
    
    // 功能配置
    bool enable_gestures;       // 启用手势
    bool enable_buttons;        // 启用按键
    bool enable_notifications;  // 启用通知
    bool enable_auto_update;    // 启用自动更新
    
    // 更新间隔
    uint32_t update_interval_ms; // 更新间隔
} MainScreenConfig;

// ==================== 主界面创建 ====================
UiComponent* main_screen_create(const char* name, const Rect* bounds);

// ==================== 配置管理 ====================
void main_screen_set_config(UiComponent* screen, const MainScreenConfig* config);
void main_screen_get_config(const UiComponent* screen, MainScreenConfig* config);

// ==================== 数据更新 ====================
void main_screen_update_time(UiComponent* screen, const struct tm* time);
void main_screen_update_location(UiComponent* screen, const char* location, 
                                const char* weather, float temperature);
void main_screen_update_battery(UiComponent* screen, uint8_t level);
void main_screen_update_bluetooth(UiComponent* screen, bool connected);
void main_screen_update_notifications(UiComponent* screen, bool has_notifications);
void main_screen_update_health_status(UiComponent* screen, const char* status, 
                                     TrendLevel trend);
void main_screen_update_last_test_time(UiComponent* screen, uint32_t timestamp);
void main_screen_update_navigation_hints(UiComponent* screen, 
                                        const char* swipe_up,
                                        const char* swipe_down,
                                        const char* button1,
                                        const char* button2);
void main_screen_update_quick_data(UiComponent* screen,
                                  uint8_t heart_rate,
                                  uint8_t blood_oxygen,
                                  float acetone_level);

// ==================== 批量更新 ====================
void main_screen_update_all(UiComponent* screen, const MainScreenData* data);

// ==================== 布局控制 ====================
void main_screen_set_layout(UiComponent* screen, const MainScreenLayout* layout);
void main_screen_get_layout(const UiComponent* screen, MainScreenLayout* layout);
void main_screen_auto_layout(UiComponent* screen);

// ==================== 可见性控制 ====================
void main_screen_show_status_bar(UiComponent* screen, bool show);
void main_screen_show_date_time(UiComponent* screen, bool show);
void main_screen_show_navigation_hints(UiComponent* screen, bool show);
void main_screen_show_health_status(UiComponent* screen, bool show);
void main_screen_show_quick_data(UiComponent* screen, bool show);
void main_screen_show_quick_actions(UiComponent* screen, bool show);

// ==================== 样式设置 ====================
void main_screen_set_background_color(UiComponent* screen, uint16_t color);
void main_screen_set_text_color(UiComponent* screen, uint16_t color);
void main_screen_set_highlight_color(UiComponent* screen, uint16_t color);
void main_screen_set_font_size(UiComponent* screen, uint8_t size);

// ==================== 动画效果 ====================
void main_screen_start_welcome_animation(UiComponent* screen);
void main_screen_stop_welcome_animation(UiComponent* screen);
void main_screen_start_status_pulse(UiComponent* screen);
void main_screen_stop_status_pulse(UiComponent* screen);
void main_screen_start_navigation_blink(UiComponent* screen);
void main_screen_stop_navigation_blink(UiComponent* screen);

// ==================== 交互反馈 ====================
void main_screen_show_swipe_up_feedback(UiComponent* screen);
void main_screen_show_swipe_down_feedback(UiComponent* screen);
void main_screen_show_button_press_feedback(UiComponent* screen, uint8_t button);
void main_screen_show_notification_indicator(UiComponent* screen, bool show);

// ==================== 工具函数 ====================
uint16_t main_screen_get_preferred_height();
void main_screen_calculate_layout(const Rect* bounds, MainScreenLayout* layout);
const char* main_screen_get_trend_text(TrendLevel trend);
uint16_t main_screen_get_trend_color(TrendLevel trend);

// ==================== 时间格式化 ====================
void main_screen_format_time(char* buffer, size_t buffer_size, const struct tm* time);
void main_screen_format_date(char* buffer, size_t buffer_size, const struct tm* time);
void main_screen_format_temperature(char* buffer, size_t buffer_size, float temp);
void main_screen_format_time_ago(char* buffer, size_t buffer_size, uint32_t timestamp);

// ==================== 默认配置 ====================
extern const MainScreenConfig MAIN_SCREEN_DEFAULT_CONFIG;
extern const MainScreenLayout MAIN_SCREEN_DEFAULT_LAYOUT;

// ==================== 预定义主界面 ====================
// 标准主界面
UiComponent* main_screen_create_standard(const char* name, const Rect* bounds);
// 简洁主界面
UiComponent* main_screen_create_minimal(const char* name, const Rect* bounds);
// 医疗主界面
UiComponent* main_screen_create_medical(const char* name, const Rect* bounds);
// 调试主界面
UiComponent* main_screen_create_debug(const char* name, const Rect* bounds);

// ==================== 事件处理 ====================
typedef void (*MainScreenCallback)(UiComponent* screen, uint8_t event_type, void* user_data);

void main_screen_set_swipe_up_callback(UiComponent* screen, MainScreenCallback callback, void* user_data);
void main_screen_set_swipe_down_callback(UiComponent* screen, MainScreenCallback callback, void* user_data);
void main_screen_set_button1_callback(UiComponent* screen, MainScreenCallback callback, void* user_data);
void main_screen_set_button2_callback(UiComponent* screen, MainScreenCallback callback, void* user_data);
void main_screen_set_notification_callback(UiComponent* screen, MainScreenCallback callback, void* user_data);

// ==================== 示例数据 ====================
extern const MainScreenData MAIN_SCREEN_EXAMPLE_DATA;

// ==================== 快速设置 ====================
void main_screen_setup_default(UiComponent* screen);
void main_screen_setup_medical(UiComponent* screen);
void main_screen_setup_debug(UiComponent* screen);

#ifdef __cplusplus
}
#endif

#endif // MAIN_SCREEN_H