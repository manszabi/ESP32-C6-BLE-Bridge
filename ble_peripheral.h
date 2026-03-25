#pragma once

#include <Arduino.h>

bool blePeripheralInit();
void blePeripheralStartAdvertising();
void blePeripheralLoop();

// Zwift → ESP32 → Suito
void handleControlFromClient(int targetPower);
