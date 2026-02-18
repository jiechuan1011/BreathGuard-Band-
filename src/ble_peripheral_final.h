#ifndef BLE_PERIPHERAL_FINAL_H
#define BLE_PERIPHERAL_FINAL_H

#include <Arduino.h>

#define BLE_DEVICE_NAME     "DiabetesSensor"

typedef struct {
    uint8_t is_connected;
    uint32_t total_notifications;
    uint32_t total_bytes_sent;
} BleStats;

void ble_peripheral_init();
void ble_peripheral_send_data();

uint8_t ble_peripheral_is_connected();
uint32_t ble_peripheral_get_notifications_sent();
void ble_peripheral_get_stats(BleStats* stats);
void ble_peripheral_print_stats();

#endif