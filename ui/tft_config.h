/*
 * tft_config.h - TFT_eSPI库配置文件
 * 
 * 针对ESP32-S3R8N8和1.85英寸AMOLED屏幕的配置
 * 屏幕规格：390×450像素，ST7789驱动，SPI接口
 */

#ifndef TFT_CONFIG_H
#define TFT_CONFIG_H

// ==================== 用户配置 ====================
// 取消注释以下行以启用相应功能

// 1. 选择显示驱动
#define ST7789_DRIVER     // 适用于1.85英寸AMOLED

// 2. 屏幕尺寸
#define TFT_WIDTH  390
#define TFT_HEIGHT 450

// 3. 旋转方向
// #define TFT_ROTATION 0   // 0° 竖屏
// #define TFT_ROTATION 1   // 90° 横屏（推荐）
// #define TFT_ROTATION 2   // 180° 竖屏
// #define TFT_ROTATION 3   // 270° 横屏

// 4. 颜色深度
#define TFT_COLOR_DEPTH 16  // 16位颜色（565格式）

// 5. SPI接口配置
#define TFT_SPI_FREQUENCY  40000000  // 40MHz SPI频率
#define TFT_SPI_MODE SPI_MODE3       // SPI模式3

// 6. 引脚定义（ESP32-S3R8N8）
#define TFT_MISO  -1        // 未使用（MISO）
#define TFT_MOSI  11        // GPIO11 - SPI MOSI
#define TFT_SCLK  12        // GPIO12 - SPI SCLK
#define TFT_CS    10        // GPIO10 - 片选
#define TFT_DC    9         // GPIO9 - 数据/命令
#define TFT_RST   8         // GPIO8 - 复位
#define TFT_BL    2         // GPIO2 - 背光控制

// 7. 触摸屏支持（如果可用）
// #define TOUCH_CS 6        // 触摸屏片选
// #define TOUCH_IRQ 5       // 触摸屏中断

// 8. 字体支持
#define LOAD_GLCD   // 默认字体
#define LOAD_FONT2  // 小字体
#define LOAD_FONT4  // 中字体
#define LOAD_FONT6  // 大字体
#define LOAD_FONT7  // 7段字体
#define LOAD_FONT8  // 大数字字体
#define LOAD_GFXFF  // 免费字体

// 9. 反锯齿字体
#define SMOOTH_FONT

// 10. 性能优化
#define SPI_FREQUENCY  40000000  // 40MHz SPI
#define SPI_READ_FREQUENCY  20000000  // 20MHz读取
#define SPI_TOUCH_FREQUENCY  2500000  // 2.5MHz触摸

// 11. 内存优化
// #define USE_HSPI_PORT      // 使用HSPI端口
// #define SUPPORT_TRANSACTIONS  // 支持SPI事务

// 12. 调试选项
// #define TFT_DEBUG          // 启用调试输出

// ==================== 自动配置 ====================
// 以下配置根据上述设置自动计算

#ifdef ST7789_DRIVER
  #ifndef TFT_WIDTH
    #define TFT_WIDTH 390
  #endif
  #ifndef TFT_HEIGHT
    #define TFT_HEIGHT 450
  #endif
  #ifndef TFT_ROTATION
    #define TFT_ROTATION 1  // 默认横屏
  #endif
#endif

// ==================== 颜色定义 ====================
// 16位颜色（565格式）
#define TFT_BLACK       0x0000
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_DARKCYAN    0x03EF
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_OLIVE       0x7BE0
#define TFT_LIGHTGREY   0xC618
#define TFT_DARKGREY    0x7BEF
#define TFT_BLUE        0x001F
#define TFT_GREEN       0x07E0
#define TFT_CYAN        0x07FF
#define TFT_RED         0xF800
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_WHITE       0xFFFF
#define TFT_ORANGE      0xFDA0
#define TFT_GREENYELLOW 0xAFE5
#define TFT_PINK        0xF81F

// ==================== 字体大小定义 ====================
#define GLCD_FONT_WIDTH   6
#define GLCD_FONT_HEIGHT  8

#define FONT2_WIDTH      8
#define FONT2_HEIGHT     16

#define FONT4_WIDTH      16
#define FONT4_HEIGHT     26

#define FONT6_WIDTH      22
#define FONT6_HEIGHT     36

#define FONT7_WIDTH      28
#define FONT7_HEIGHT     48

#define FONT8_WIDTH      34
#define FONT8_HEIGHT     58

// ==================== 工具函数 ====================
// 颜色转换函数
static inline uint16_t rgb_to_565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline void rgb_from_565(uint16_t color, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = (color >> 11) & 0x1F;
    *g = (color >> 5) & 0x3F;
    *b = color & 0x1F;
    
    // 扩展到8位
    *r = (*r * 255) / 31;
    *g = (*g * 255) / 63;
    *b = (*b * 255) / 31;
}

// 亮度调整函数
static inline uint16_t adjust_brightness(uint16_t color, uint8_t brightness) {
    if (brightness >= 100) return color;
    
    uint8_t r, g, b;
    rgb_from_565(color, &r, &g, &b);
    
    r = (r * brightness) / 100;
    g = (g * brightness) / 100;
    b = (b * brightness) / 100;
    
    return rgb_to_565(r, g, b);
}

// 颜色混合函数
static inline uint16_t blend_colors(uint16_t color1, uint16_t color2, uint8_t alpha) {
    uint8_t r1, g1, b1, r2, g2, b2;
    rgb_from_565(color1, &r1, &g1, &b1);
    rgb_from_565(color2, &r2, &g2, &b2);
    
    uint8_t r = (r1 * (255 - alpha) + r2 * alpha) / 255;
    uint8_t g = (g1 * (255 - alpha) + g2 * alpha) / 255;
    uint8_t b = (b1 * (255 - alpha) + b2 * alpha) / 255;
    
    return rgb_to_565(r, g, b);
}

// ==================== 屏幕工具函数 ====================
// 坐标转换（考虑旋转）
static inline void apply_rotation(int16_t* x, int16_t* y) {
    int16_t temp;
    
    switch (TFT_ROTATION) {
        case 1:
            temp = *x;
            *x = TFT_WIDTH - 1 - *y;
            *y = temp;
            break;
        case 2:
            *x = TFT_WIDTH - 1 - *x;
            *y = TFT_HEIGHT - 1 - *y;
            break;
        case 3:
            temp = *x;
            *x = *y;
            *y = TFT_HEIGHT - 1 - temp;
            break;
        default:
            // 0度旋转，无需转换
            break;
    }
}

// 检查坐标是否在屏幕内
static inline bool is_point_inside(int16_t x, int16_t y) {
    return (x >= 0 && x < TFT_WIDTH && y >= 0 && y < TFT_HEIGHT);
}

// 计算两点距离
static inline float distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    int16_t dx = x2 - x1;
    int16_t dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

// ==================== 医疗UI颜色定义 ====================
// 这些颜色与ui_config.h保持一致
#define MEDICAL_NORMAL    TFT_GREEN      // 正常值
#define MEDICAL_WARNING   TFT_ORANGE     // 警告值
#define MEDICAL_DANGER    TFT_RED        // 危险值
#define MEDICAL_INFO      TFT_CYAN       // 信息
#define MEDICAL_SUCCESS   TFT_GREEN      // 成功

#define UI_BACKGROUND     TFT_BLACK      // 背景色
#define UI_TEXT_PRIMARY   TFT_WHITE      // 主要文字
#define UI_TEXT_SECONDARY TFT_LIGHTGREY  // 次要文字
#define UI_CARD_BG        0x1082         // 卡片背景
#define UI_BORDER         0x3186         // 边框颜色

// ==================== 字体大小定义 ====================
#define FONT_SIZE_TITLE   24  // 主标题
#define FONT_SIZE_BODY    16  // 正文
#define FONT_SIZE_SMALL   12  // 小字
#define FONT_SIZE_LARGE   32  // 大数字

// ==================== 初始化函数 ====================
// 此函数应在setup()中调用
void init_tft_display() {
    // 初始化引脚
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_RST, OUTPUT);
    pinMode(TFT_BL, OUTPUT);
    
    // 设置初始状态
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(TFT_DC, HIGH);
    digitalWrite(TFT_RST, HIGH);
    digitalWrite(TFT_BL, LOW);  // 背光关闭
    
    // 初始化SPI
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    
    // 配置SPI频率
    SPI.setFrequency(TFT_SPI_FREQUENCY);
    
    // 初始化显示（实际初始化在TFT_eSPI库中完成）
    
    // 开启背光
    digitalWrite(TFT_BL, HIGH);
    
    Serial.println("[TFT] 显示初始化完成");
    Serial.printf("[TFT] 分辨率: %dx%d, 旋转: %d度\n", 
                  TFT_WIDTH, TFT_HEIGHT, TFT_ROTATION * 90);
}

// ==================== 背光控制 ====================
void set_backlight_brightness(uint8_t percent) {
    if (percent == 0) {
        digitalWrite(TFT_BL, LOW);
    } else if (percent >= 100) {
        digitalWrite(TFT_BL, HIGH);
    } else {
        // PWM控制背光
        analogWrite(TFT_BL, map(percent, 0, 100, 0, 255));
    }
}

// ==================== 屏幕保护 ====================
void enable_screen_saver(bool enable) {
    static bool screen_saver_enabled = false;
    
    if (enable && !screen_saver_enabled) {
        // 启用屏幕保护
        // 可以在这里实现像素偏移、亮度降低等
        screen_saver_enabled = true;
        Serial.println("[TFT] 屏幕保护已启用");
    } else if (!enable && screen_saver_enabled) {
        // 禁用屏幕保护
        screen_saver_enabled = false;
        Serial.println("[TFT] 屏幕保护已禁用");
    }
}

// ==================== 性能监控 ====================
#ifdef TFT_DEBUG
void print_tft_stats() {
    static uint32_t last_print_time = 0;
    static uint32_t frame_count = 0;
    
    uint32_t current_time = millis();
    frame_count++;
    
    if (current_time - last_print_time >= 1000) {
        float fps = (float)frame_count * 1000.0f / (current_time - last_print_time);
        Serial.printf("[TFT] 帧率: %.1f fps\n", fps);
        
        last_print_time = current_time;
        frame_count = 0;
    }
}
#endif

#endif // TFT_CONFIG_H