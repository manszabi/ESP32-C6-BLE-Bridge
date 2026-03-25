#include "ble_central.h"
#include "data_model.h"
#include "config.h"

#include <NimBLEDevice.h>

static NimBLEClient* suitoClient = nullptr;

bool bleCentralInit() {
    NimBLEDevice::init("ESP32C6-Bridge-Central");
    NimBLEDevice::setPower(ESP_PWR_LVL_P7);
    return true;
}

bool bleCentralConnectToSuito() {
    suitoClient = NimBLEDevice::createClient();

    Serial.println("Connecting to Suito...");
    if (!suitoClient->connect(SUITO_MAC_ADDRESS)) {
        Serial.println("Failed to connect to Suito!");
        return false;
    }

    Serial.println("Connected to Suito!");

    // TODO: FTMS service keresése + Indoor Bike Data subscribe

    return true;
}

void bleCentralLoop() {
    // TODO: FTMS notify-k feldolgozása
}

void sendResistanceCommandToSuito(int targetPower) {
    // TODO: Control Point write
}
