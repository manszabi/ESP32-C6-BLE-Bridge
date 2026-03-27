#pragma once
#include <Arduino.h>

// Zwift → ESP32 → Suito vezérlés
// Fogadja a kliens parancsokat és továbbítja a Suito felé

void handleControlFromClient(int targetPower);
