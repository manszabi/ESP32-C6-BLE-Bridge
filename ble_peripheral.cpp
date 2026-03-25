#include "ble_peripheral.h"
#include "data_model.h"
#include "config.h"

#include <NimBLEDevice.h>

static NimBLEServer* bleServer = nullptr;

static NimBLECharacteristic* ftmsDataChar = nullptr;
static NimBLECharacteristic* cpsPowerChar = nullptr;
static NimBLECharacteristic* cscMeasureChar = nullptr;

bool blePeripheralInit() {
    NimBLEDevice::init(BRIDGE_DEVICE_NAME);

    bleServer = NimBLEDevice::createServer();

    // TODO: FTMS, CPS, CSC service-ek létrehozása

    return true;
}

void blePeripheralStartAdvertising() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID("1826"); // FTMS
    adv->addServiceUUID("1818"); // CPS
    adv->addServiceUUID("1816"); // CSC
    adv->setScanResponse(true);
    adv->start();
    Serial.println("BLE advertising started.");
}

void blePeripheralLoop() {
    TrainerData d = g_trainerData;

    // TODO: FTMS notify
    // TODO: CPS notify
    // TODO: CSC notify
}

void handleControlFromClient(int targetPower) {
    // TODO: Zwift → ESP32 → Suito Control Point
}
