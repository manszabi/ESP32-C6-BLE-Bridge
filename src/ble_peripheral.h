#pragma once

#include <Arduino.h>

// Inicializáció / fő loop
bool blePeripheralInit();
void blePeripheralStartAdvertising();
void blePeripheralLoop();    // opcionális: ha van per‑loop munka (pl. állapotellenőrzés)

// Notify küldő függvények (scheduler hívja)
void blePeripheralSendFtms();
void blePeripheralSendCps();
void blePeripheralSendCsc();

// Kliens-ellenőrzés (scheduler használja, hogy elkerüljük a felesleges notify-kat)
bool isFtmsClientConnected();
bool isCpsClientConnected();
bool isCscClientConnected();

// Zwift → ESP32 → Suito vezérlés (FTMS Control Pointból hívva)
void handleControlFromClient(int targetPower);

// Utility: ha szükséges, visszaadjuk, hogy a peripheral BLE szerver fut-e
bool blePeripheralIsRunning();
