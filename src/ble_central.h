#pragma once
#include <Arduino.h>

// BLE Central modul — Suito FTMS kapcsolat + scanner

bool bleCentralInit();
bool bleCentralConnectToSuito();
bool bleCentralIsConnected();
void sendResistanceCommandToSuito(int targetPower);

// Disconnect esemény flag (scheduler figyeli)
bool bleCentralWasDisconnected();

// Scanner
bool bleScanStart();
bool bleScanIsRunning();
bool bleScanFtmsFound();
void bleScanReset();  // Elfelejtett cím törlése, újrakeresés
