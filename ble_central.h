#pragma once

#include <Arduino.h>

bool bleCentralInit();
bool bleCentralConnectToSuito();
void bleCentralLoop();

// Zwift → ESP32 → Suito (Control Point)
void sendResistanceCommandToSuito(int targetPower);
