/*
 * status_bar.h - 状态栏组件头文件
 * 
 * 功能：显示时间、电量、连接状态、通知图标
 * 设计原则：
 * 1. 固定在屏幕顶部
 * 2. 实时更新信息
 * 3. 低功耗显示
 * 4. 医疗状态指示
 */

#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include "ui_component.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 状态栏图标类型 ====================
typedef enum {
    ICON_BATTERY = 0,
    ICON_BLUETOOTH,
    ICON_WIFI,
    ICON_NOTIFICATION,
    ICON_ALARM,
    ICON_HEART,
    ICON_ACETONE,
    ICON_WARNING,
    ICON_COUNT
} StatusBarIcon;

// ==================== 电池状态 ====================
typedef enum {
    BATTERY_UNKNOWN = 0,
    BATTERY_CHARGING,
    BATTERY_FULL,
    BATTERY_HIGH,      // 75-100%
    BATTERY_MEDIUM,    // 50-75%
    BATTERY_LOW,       // 25-50%
    BATTERY_CRITICAL   // 0-25%
} BatteryState;

// ==================== 连接状态 ====================
typedef enum {
    CONNECTION_DISCONNECTED = 0,
    CONNECTION_CONNECTING,
    CONNECTION_CONNECTED
} ConnectionState;

// ==================== 状态栏配置 ====================
typedef struct {
    bool show_time;             // 显示时间
    bool show_date;             // 显示日期
    bool show_battery;          // 显示电量
    bool show_bluetooth;        // 显示蓝牙状态
    bool show_notifications;    // 显示通知
    bool show_medical_status;   // 显示医疗状态
    uint8_t update_interval_ms; // 更新间隔
} StatusBarConfig;

// ==================== 状态栏数据 ====================
typedef struct {
    // 时间信息
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    const char* weekday;        // 星期几
    
    // 系统状态
    uint8_t battery_level;      // 电量百分比 0-100
    BatteryState battery_state; // 电池状态
    ConnectionState bluetooth_state; // 蓝牙状态
    bool has_notifications;     // 是否有通知
    bool medical_alert;         // 医疗警告
    
    // 医疗数据
    uint8_t heart_rate;         // 心率
    float acetone_level;        // 丙酮浓度
    TrendLevel trend;           // 趋势
} StatusBarData;

// ==================== 状态栏组件创建 ====================
UiComponent* status_bar_create(const char* name, const Rect* bounds);

// ==================== 配置管理 ====================
void status_bar_set_config(UiComponent* status_bar, const StatusBarConfig* config);
void status_bar_get_config(const UiComponent* status_bar, StatusBarConfig* config);

// ==================== 数据更新 ====================
void status_bar_update_time(UiComponent* status_bar, 
                           uint8_t hour, uint8_t minute, uint8_t second);
void status_bar_update_date(UiComponent* status_bar,
                           uint8_t day, uint8_t month, uint16_t year,
                           const char* weekday);
void status_bar_update_battery(UiComponent* status_bar,
                              uint8_t level, BatteryState state);
void status_bar_update_bluetooth(UiComponent* status_bar,
                                ConnectionState state);
void status_bar_update_notifications(UiComponent* status_bar,
                                    bool has_notifications);
void status_bar_update_medical_status(UiComponent* status_bar,
                                     uint8_t heart_rate,
                                     float acetone_level,
                                     TrendLevel trend,
                                     bool alert);

// ==================== 批量更新 ====================
void status_bar_update_all(UiComponent* status_bar,
                          const StatusBarData* data);

// ==================== 样式设置 ====================
void status_bar_set_time_format(UiComponent* status_bar, bool is_24h);
void status_bar_set_date_format(UiComponent* status_bar, bool show_year);
void status_bar_set_icon_size(UiComponent* status_bar, uint8_t size);
void status_bar_set_text_color(UiComponent* status_bar, uint16_t color);
void status_bar_set_icon_color(UiComponent* status_bar, uint16_t color);
void status_bar_set_warning_color(UiComponent* status_bar, uint16_t color);

// ==================== 可见性控制 ====================
void status_bar_show_icon(UiComponent* status_bar, StatusBarIcon icon, bool show);
bool status_bar_is_icon_visible(const UiComponent* status_bar, StatusBarIcon icon);

// ==================== 动画效果 ====================
void status_bar_start_blink(UiComponent* status_bar, StatusBarIcon icon, uint16_t interval_ms);
void status_bar_stop_blink(UiComponent* status_bar, StatusBarIcon icon);
void status_bar_start_pulse(UiComponent* status_bar, StatusBarIcon icon);
void status_bar_stop_pulse(UiComponent* status_bar, StatusBarIcon icon);

// ==================== 工具函数 ====================
uint16_t status_bar_get_preferred_height();
void status_bar_auto_adjust_layout(UiComponent* status_bar);
bool status_bar_is_medical_alert_active(const UiComponent* status_bar);

// ==================== 电池图标辅助函数 ====================
uint16_t status_bar_get_battery_color(uint8_t level);
const char* status_bar_get_battery_icon(uint8_t level, BatteryState state);

// ==================== 时间格式化 ====================
void status_bar_format_time(char* buffer, size_t buffer_size,
                           uint8_t hour, uint8_t minute, bool is_24h);
void status_bar_format_date(char* buffer, size_t buffer_size,
                           uint8_t day, uint8_t month, uint16_t year,
                           bool show_year);

// ==================== 默认配置 ====================
extern const StatusBarConfig STATUS_BAR_DEFAULT_CONFIG;

#ifdef __cplusplus
}
#endif

#endif // STATUS_BAR_H