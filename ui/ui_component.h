/*
 * ui_component.h - UI组件基类头文件
 * 
 * 功能：所有UI组件的基类，提供统一的接口
 * 设计原则：
 * 1. 组件化设计
 * 2. 统一的绘制和更新接口
 * 3. 事件处理机制
 * 4. 内存管理
 */

#ifndef UI_COMPONENT_H
#define UI_COMPONENT_H

#include "ui_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 组件类型 ====================
typedef enum {
    COMPONENT_TYPE_BASE = 0,
    COMPONENT_TYPE_STATUS_BAR,
    COMPONENT_TYPE_METRIC_CARD,
    COMPONENT_TYPE_PROGRESS_RING,
    COMPONENT_TYPE_WAVEFORM_VIEW,
    COMPONENT_TYPE_HISTORY_TIMELINE,
    COMPONENT_TYPE_ALERT_DIALOG,
    COMPONENT_TYPE_BREATHING_GUIDE,
    COMPONENT_TYPE_BUTTON,
    COMPONENT_TYPE_LABEL,
    COMPONENT_TYPE_ICON
} ComponentType;

// ==================== 组件状态 ====================
typedef enum {
    COMPONENT_STATE_NORMAL = 0,
    COMPONENT_STATE_HOVER,
    COMPONENT_STATE_PRESSED,
    COMPONENT_STATE_DISABLED,
    COMPONENT_STATE_HIDDEN
} ComponentState;

// ==================== 对齐方式 ====================
typedef enum {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT,
    ALIGN_TOP,
    ALIGN_MIDDLE,
    ALIGN_BOTTOM
} Alignment;

// ==================== 组件事件 ====================
typedef enum {
    COMPONENT_EVENT_NONE = 0,
    COMPONENT_EVENT_CLICK,
    COMPONENT_EVENT_LONG_PRESS,
    COMPONENT_EVENT_VALUE_CHANGED,
    COMPONENT_EVENT_ANIMATION_START,
    COMPONENT_EVENT_ANIMATION_END
} ComponentEvent;

// ==================== 事件回调函数类型 ====================
typedef void (*ComponentEventCallback)(void* component, ComponentEvent event, void* user_data);

// ==================== 组件基类结构 ====================
typedef struct UiComponent {
    // 基本信息
    ComponentType type;
    char name[32];
    
    // 几何属性
    Rect bounds;
    Rect clip_rect;
    
    // 状态
    ComponentState state;
    bool visible;
    bool enabled;
    bool needs_redraw;
    
    // 样式
    uint16_t background_color;
    uint16_t foreground_color;
    uint16_t border_color;
    uint8_t border_width;
    uint8_t border_radius;
    uint8_t opacity; // 0-255
    
    // 事件处理
    ComponentEventCallback event_callback;
    void* user_data;
    
    // 子组件管理
    struct UiComponent** children;
    uint8_t child_count;
    uint8_t child_capacity;
    
    // 父组件
    struct UiComponent* parent;
    
    // 虚拟函数表
    void (*destroy)(struct UiComponent* self);
    void (*update)(struct UiComponent* self, uint32_t delta_time);
    void (*render)(struct UiComponent* self, void* display);
    bool (*handle_event)(struct UiComponent* self, uint8_t event_type, void* event_data);
    void (*set_position)(struct UiComponent* self, int16_t x, int16_t y);
    void (*set_size)(struct UiComponent* self, uint16_t width, uint16_t height);
    void (*set_bounds)(struct UiComponent* self, const Rect* bounds);
    void (*add_child)(struct UiComponent* self, struct UiComponent* child);
    void (*remove_child)(struct UiComponent* self, struct UiComponent* child);
    void (*bring_to_front)(struct UiComponent* self);
    void (*send_to_back)(struct UiComponent* self);
    
    // 私有数据
    void* private_data;
} UiComponent;

// ==================== 创建和销毁 ====================
UiComponent* ui_component_create(ComponentType type, const char* name);
void ui_component_destroy(UiComponent* component);

// ==================== 基础操作 ====================
void ui_component_set_position(UiComponent* component, int16_t x, int16_t y);
void ui_component_set_size(UiComponent* component, uint16_t width, uint16_t height);
void ui_component_set_bounds(UiComponent* component, const Rect* bounds);
void ui_component_set_visible(UiComponent* component, bool visible);
void ui_component_set_enabled(UiComponent* component, bool enabled);
void ui_component_set_opacity(UiComponent* component, uint8_t opacity);

// ==================== 样式设置 ====================
void ui_component_set_background_color(UiComponent* component, uint16_t color);
void ui_component_set_foreground_color(UiComponent* component, uint16_t color);
void ui_component_set_border_color(UiComponent* component, uint16_t color);
void ui_component_set_border_width(UiComponent* component, uint8_t width);
void ui_component_set_border_radius(UiComponent* component, uint8_t radius);

// ==================== 事件处理 ====================
void ui_component_set_event_callback(UiComponent* component, 
                                    ComponentEventCallback callback, 
                                    void* user_data);
bool ui_component_handle_event(UiComponent* component, uint8_t event_type, void* event_data);

// ==================== 子组件管理 ====================
void ui_component_add_child(UiComponent* parent, UiComponent* child);
void ui_component_remove_child(UiComponent* parent, UiComponent* child);
void ui_component_remove_all_children(UiComponent* parent);
UiComponent* ui_component_get_child_at(UiComponent* parent, int16_t x, int16_t y);
void ui_component_bring_to_front(UiComponent* component);
void ui_component_send_to_back(UiComponent* component);

// ==================== 更新和渲染 ====================
void ui_component_update(UiComponent* component, uint32_t delta_time);
void ui_component_render(UiComponent* component, void* display);
void ui_component_force_redraw(UiComponent* component);

// ==================== 工具函数 ====================
bool ui_component_is_point_inside(const UiComponent* component, int16_t x, int16_t y);
Rect ui_component_get_absolute_bounds(const UiComponent* component);
void ui_component_set_clip_rect(UiComponent* component, const Rect* clip_rect);
void ui_component_clear_clip_rect(UiComponent* component);

// ==================== 内存管理 ====================
uint32_t ui_component_get_memory_usage(const UiComponent* component);

#ifdef __cplusplus
}
#endif

#endif // UI_COMPONENT_H