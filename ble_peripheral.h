#pragma once

#include <Arduino.h>

bool blePeripheralInit();
bool isFtmsClientConnected();
bool isCpsClientConnected();
bool isCscClientConnected();

void blePeripheralStartAdvertising();
void blePeripheralLoop();

// Zwift → ESP32 → Suito
void handleControlFromClient(int targetPower);
