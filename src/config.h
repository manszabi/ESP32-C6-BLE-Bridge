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

// HRM és Cadence szenzor (opcionális MAC, ha üres, UUID scan-nel keres)
static const char* HRM_MAC_ADDRESS     = "";   // üres = automatikus scan
static const char* CADENCE_MAC_ADDRESS = "";   // üres = automatikus scan

// Kerék kerülete (CSC számoláshoz)
static const float WHEEL_CIRCUMFERENCE_M = 2.105f;  // 700x25c

// Scan timeout
static const uint32_t BLE_SCAN_DURATION_SEC = 5;

// Debug
#define DEBUG_SERIAL 1