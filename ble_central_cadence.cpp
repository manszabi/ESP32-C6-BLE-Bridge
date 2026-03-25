#include "ble_central_cadence.h"
#include "data_model.h"
#include <NimBLEDevice.h>

// Cadence szenzor (CSC 0x1816, Measurement 0x2A5B)
static NimBLEClient* cadClient = nullptr;

bool bleCentralCadenceInit() {
    // ugyanaz a NimBLEDevice
    return true;
}

bool bleCentralCadenceConnect() {
    // TODO: scan cadence szenzorra, connect, subscribe 0x2A5B-re
    return true;
}

void bleCentralCadenceLoop() {
    // notify callbackben:
    // updateExternalCadence(cadence);
}

bool bleCentralCadenceIsConnected() {
    return cadClient && cadClient->isConnected();
}
