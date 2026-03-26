#pragma once

#include <Arduino.h>

// Suito MAC címe (ha tudod)
static const char* SUITO_MAC_ADDRESS = "AA:BB:CC:DD:EE:FF";

// BLE eszköz neve
static const char* BRIDGE_DEVICE_NAME = "ESP32C6-Suito-Bridge";

// Ki legyen a vezérlő (Zwift)
static const char* CONTROL_MASTER_NAME = "ZWIFT";

// Időzítések
static const uint16_t CONNECTION_INTERVAL_MS = 20;
static const uint16_t NOTIFY_INTERVAL_MS     = 100;

// Debug
#define DEBUG_SERIAL 1

constexpr float WHEEL_CIRCUMFERENCE_M = 2.105f;