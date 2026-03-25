#pragma once
#include <Arduino.h>

void blePeripheralLoop();

bool blePeripheralInit();
void blePeripheralStartAdvertising();

// Zwift / Garmin / Telefon felé küldött notify-k
void blePeripheralSendFtms();
void blePeripheralSendCps();
void blePeripheralSendCsc();

// Kliens-ellenőrzés
bool isFtmsClientConnected();
bool isCpsClientConnected();
bool isCscClientConnected();

// Zwift → ESP32 → Suito vezérlés
void handleControlFromClient(int targetPower);
