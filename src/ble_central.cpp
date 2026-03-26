#include "ble_central.h"
#include "config.h"
#include "ftms_parser.h"
#include "ble_scan.h"

#include <NimBLEDevice.h>

// ───────────────────────────────────────────────
// Suito Central modul
// Service: 0x1826 (FTMS)
// Characteristics:
//   0x2AD2 - Indoor Bike Data (notify)
//   0x2AD9 - FTMS Control Point (write)
// ───────────────────────────────────────────────

static NimBLEClient* suitoClient = nullptr;
static NimBLERemoteCharacteristic* ftmsIndoorBikeDataChar = nullptr;
static NimBLERemoteCharacteristic* ftmsControlPointChar   = nullptr;

// Cím a unified scannerből
extern NimBLEAddress* foundFtmsAddress;

// ───────────────────────────────────────────────
// Indoor Bike Data notify callback
// ───────────────────────────────────────────────

static void onFtmsNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    parseFtmsIndoorBikeData(data, len);
}

// ───────────────────────────────────────────────
// Init
// ───────────────────────────────────────────────

bool bleCentralInit() {
    NimBLEDevice::init(BRIDGE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    return true;
}

// ───────────────────────────────────────────────
// Connect — unified scanner-ből kapott címmel
// ───────────────────────────────────────────────

bool bleCentralConnectToSuito() {
    if (suitoClient && suitoClient->isConnected()) {
        return true;
    }

    // Várjuk meg, amíg a unified scanner megtalálja
    if (!foundFtmsAddress) {
        return false;
    }

    // Klienst csak egyszer hozzuk létre
    if (!suitoClient) {
        suitoClient = NimBLEDevice::createClient();
        if (!suitoClient) {
            Serial.println("Failed to create BLE client for Suito!");
            return false;
        }
    }

    Serial.println("Connecting to FTMS trainer...");
    if (!suitoClient->connect(*foundFtmsAddress)) {
        Serial.println("Failed to connect to FTMS trainer!");
        return false;
    }

    Serial.println("Connected, discovering FTMS services...");

    NimBLERemoteService* ftmsService = suitoClient->getService("1826");
    if (!ftmsService) {
        Serial.println("FTMS service not found!");
        suitoClient->disconnect();
        return false;
    }

    ftmsIndoorBikeDataChar = ftmsService->getCharacteristic("2AD2");
    if (!ftmsIndoorBikeDataChar) {
        Serial.println("Indoor Bike Data characteristic not found!");
        suitoClient->disconnect();
        return false;
    }

    ftmsControlPointChar = ftmsService->getCharacteristic("2AD9");
    if (!ftmsControlPointChar) {
        Serial.println("Control Point characteristic not found (non-fatal).");
    }

    if (ftmsIndoorBikeDataChar->canNotify()) {
        ftmsIndoorBikeDataChar->subscribe(true, onFtmsNotify);
        Serial.println("Subscribed to Indoor Bike Data notifications.");
    } else {
        Serial.println("Indoor Bike Data characteristic cannot notify!");
        suitoClient->disconnect();
        return false;
    }

    return true;
}

// ───────────────────────────────────────────────
// Loop — callback-alapú
// ───────────────────────────────────────────────

void bleCentralLoop() {
}

// ───────────────────────────────────────────────
// Állapot lekérdezés
// ───────────────────────────────────────────────

bool bleCentralIsConnected() {
    return suitoClient && suitoClient->isConnected();
}

// ───────────────────────────────────────────────
// Zwift → ESP32 → Suito vezérlés
// ───────────────────────────────────────────────

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
