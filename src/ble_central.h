#pragma once
#include <Arduino.h>

// ───────────────────────────────────────────────
// Unified BLE Central modul
// Tartalmazza: Suito FTMS, HRM, Cadence, Scanner
// ───────────────────────────────────────────────

// Inicializálás (NimBLE init + scan setup)
bool bleCentralInit();

// Suito FTMS
bool bleCentralConnectToSuito();
bool bleCentralIsConnected();
void sendResistanceCommandToSuito(int targetPower);

// HRM
bool bleCentralHrmConnect();
bool bleCentralHrmIsConnected();

// Cadence
bool bleCentralCadenceConnect();
bool bleCentralCadenceIsConnected();

// Unified Scanner
struct ScanResults {
    bool ftmsFound;
    bool hrmFound;
    bool cadenceFound;
};

bool bleScanStart();
bool bleScanIsRunning();
ScanResults bleScanGetResults();
