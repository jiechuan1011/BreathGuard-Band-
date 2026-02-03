#include "debug.h"

DebugLevel g_debug_level = DEBUG_INFO;

void debug_set_level(DebugLevel level) {
    g_debug_level = level;
}

void debug_print(DebugLevel level, const char* format, ...) {
    if (level > g_debug_level) return;

    va_list args;
    va_start(args, format);

    vprintf(format, args);
    printf("\n");

    va_end(args);
}