#pragma once
#include <Arduino.h>

// ───────────────────────────────────────────────
// Unified BLE Scanner
// Egyetlen scan-nel keresi az összes szenzort:
//   - FTMS trainer (0x1826)
//   - HRM szenzor (0x180D)
//   - Cadence szenzor (0x1816)
// ───────────────────────────────────────────────

struct ScanResults {
    bool ftmsFound;
    bool hrmFound;
    bool cadenceFound;
};

// Inicializálás
void bleScanInit();

// Egyetlen scan indítása (nem blokkoló)
// Visszaadja, hogy fut-e már scan
bool bleScanStart();

// Fut-e jelenleg scan?
bool bleScanIsRunning();

// Eredmények lekérdezése
ScanResults bleScanGetResults();
