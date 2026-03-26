#include "ble_central_hrm.h"
#include "data_model.h"
#include <NimBLEDevice.h>

// Egyszerű váz – konkrét scan/UUID keresés később
static NimBLEClient* hrmClient = nullptr;

bool bleCentralHrmInit() {
    // ugyanazt a NimBLEDevice-et használjuk, mint a többi central
    return true;
}

bool bleCentralHrmConnect() {
    // TODO: scan HRM-re (UUID 0x180D), connect, subscribe 0x2A37-re
    // hrmClient = NimBLEDevice::createClient();
    // ...
    return true;
}

void bleCentralHrmLoop() {
    // HRM notify callbackben hívod majd:
    // updateHeartRate(hr);
}

bool bleCentralHrmIsConnected() {
    return hrmClient && hrmClient->isConnected();
}
