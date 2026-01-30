/*
 * main_esp32s3.cpp - 糖尿病初筛腕带主控板（ESP32-S3R8N8完整适配版）
 * 
 * 功能：完整手表功能 + BLE广播数据
 *   - MAX30102心率血氧采集（使用现有hr_driver.cpp）
 *   - SnO₂+AD623丙酮检测（腕带无此功能，数据填-1）
 *   - BLE广播JSON数据到MIT App Inventor APP
 *   - OLED显示（超时熄屏）
 *   - 低功耗优化（ESP32-S3轻睡模式）
 * 
