#pragma once
#include <Arduino.h>

// BLE Peripheral modul
// FTMS, CPS, CSC service-ek hirdetése kliensek felé

bool blePeripheralInit();
void blePeripheralStartAdvertising();

void blePeripheralSendFtms();
void blePeripheralSendCps();
void blePeripheralSendCsc();

bool isFtmsClientConnected();
bool isCpsClientConnected();
bool isCscClientConnected();
