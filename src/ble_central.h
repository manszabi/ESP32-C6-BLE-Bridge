#pragma once
#include <Arduino.h>

// BLE Central modul — Suito FTMS kapcsolat + scanner

bool bleCentralInit();
bool bleCentralConnectToSuito();
bool bleCentralIsConnected();
void sendResistanceCommandToSuito(int targetPower);

// Scanner
bool bleScanStart();
bool bleScanIsRunning();
bool bleScanFtmsFound();
