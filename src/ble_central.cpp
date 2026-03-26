#include "ble_central.h"
#include "config.h"
#include "ftms_parser.h"

#include <NimBLEDevice.h>

static NimBLEClient* suitoClient = nullptr;
static NimBLERemoteCharacteristic* ftmsIndoorBikeDataChar = nullptr;
static NimBLERemoteCharacteristic* ftmsControlPointChar   = nullptr;

// Indoor Bike Data notify callback (NimBLE 2.x: lambda)
static void onFtmsNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    parseFtmsIndoorBikeData(data, len);
}

bool bleCentralInit() {
    NimBLEDevice::init("ESP32C6-Bridge-Central");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    return true;
}

bool bleCentralConnectToSuito() {
    if (suitoClient && suitoClient->isConnected()) {
        return true;
    }

    suitoClient = NimBLEDevice::createClient();

    Serial.println("Connecting to Suito...");
    if (!suitoClient->connect(SUITO_MAC_ADDRESS)) {
        Serial.println("Failed to connect to Suito!");
        return false;
    }

    Serial.println("Connected to Suito, discovering services...");

    NimBLERemoteService* ftmsService = suitoClient->getService("1826");
    if (!ftmsService) {
        Serial.println("FTMS service not found on Suito!");
        return false;
    }

    ftmsIndoorBikeDataChar = ftmsService->getCharacteristic("2AD2");
    if (!ftmsIndoorBikeDataChar) {
        Serial.println("Indoor Bike Data characteristic not found!");
        return false;
    }

    ftmsControlPointChar = ftmsService->getCharacteristic("2AD9");
    if (!ftmsControlPointChar) {
        Serial.println("Control Point characteristic not found!");
    }

    if (ftmsIndoorBikeDataChar->canNotify()) {
        ftmsIndoorBikeDataChar->subscribe(true, onFtmsNotify);
        Serial.println("Subscribed to Indoor Bike Data notifications.");
    } else {
        Serial.println("Indoor Bike Data characteristic cannot notify!");
    }

    return true;
}

void bleCentralLoop() {
}

bool bleCentralIsConnected() {
    return suitoClient && suitoClient->isConnected();
}

void sendResistanceCommandToSuito(int targetPower) {
    if (!ftmsControlPointChar || !suitoClient || !suitoClient->isConnected()) {
        return;
    }

    uint8_t frame[3] = {0};
    frame[0] = 0x05;
    frame[1] = targetPower & 0xFF;
    frame[2] = (targetPower >> 8) & 0xFF;

    ftmsControlPointChar->writeValue(frame, sizeof(frame), true);
}