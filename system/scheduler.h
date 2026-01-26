#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>

// ──────────────────────────────────────────────
// 函数声明
void scheduler_init();    // 初始化所有传感器
void scheduler_run();      // 主调度循环（非阻塞，需在loop()中持续调用）

#endif
