/*
 * main_control_new.ino - 糖尿病初筛腕带主控板（ESP32-S3R8N8）
 * 
 * 功能：腕带作为BLE Peripheral（服务器）发送数据
 *   - 使用NimBLE库（更省电、RAM占用低）
 *   - 定时从system_state_get()获取心率、血氧数据
 *   - 打包成JSON格式通过BLE notify发送
 *   - OLED显示当前状态
 *   - 低功耗优化
 * 
 * 硬件：
 *   - ESP32-S3R8N8（双核240MHz，8MB Flash + 8MB PSRAM）
 *   - 0.96寸 OLED SSD1306（I2C，0x3C地址）
 *   - MAX30102 心率血氧传感器
 *   - GPIO21/22 用于I2C（默认ESP32-S3引脚）
 * 
 * BLE配置：
 *   - 设备名："DiabetesSensor"
 *   - 服务UUID："a1b2c3d4-e5f6-4789-abcd-ef0123456789"
