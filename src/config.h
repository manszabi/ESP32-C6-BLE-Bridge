#pragma once

#include <Arduino.h>

// BLE eszköz neve
static const char* BRIDGE_DEVICE_NAME = "ESP32C6-Suito-Bridge";

// Ki legyen a vezérlő (Zwift)
static const char* CONTROL_MASTER_NAME = "ZWIFT";

// Időzítések
static const uint16_t CONNECTION_INTERVAL_MS = 20;
static const uint16_t NOTIFY_INTERVAL_MS     = 100;

// Kerék kerülete (CSC számoláshoz)
static const float WHEEL_CIRCUMFERENCE_M = 2.105f;  // 700x25c

// Scan timeout
static const uint32_t BLE_SCAN_DURATION_SEC = 5;

// Debug
#define DEBUG_SERIAL 1