#include "ble_central.h"
#include "config.h"
#include "ftms_parser.h"

#include <NimBLEDevice.h>

// ═══════════════════════════════════════════════
// Suito Central (FTMS)
// ═══════════════════════════════════════════════

static NimBLEClient* suitoClient = nullptr;
static NimBLERemoteCharacteristic* ftmsIndoorBikeDataChar = nullptr;
static NimBLERemoteCharacteristic* ftmsControlPointChar   = nullptr;

static NimBLEAddress* foundFtmsAddress = nullptr;

static void onFtmsNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    parseFtmsIndoorBikeData(data, len);
}

bool bleCentralConnectToSuito() {
    if (suitoClient && suitoClient->isConnected()) {
        return true;
    }

    if (!foundFtmsAddress) {
        return false;
    }

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

    NimBLERemoteService* ftmsService = suitoClient->getService(NimBLEUUID((uint16_t)0x1826));
    if (!ftmsService) {
        Serial.println("FTMS service not found!");
        suitoClient->disconnect();
        return false;
    }

    ftmsIndoorBikeDataChar = ftmsService->getCharacteristic(NimBLEUUID((uint16_t)0x2AD2));
    if (!ftmsIndoorBikeDataChar) {
        Serial.println("Indoor Bike Data characteristic not found!");
        suitoClient->disconnect();
        return false;
    }

    ftmsControlPointChar = ftmsService->getCharacteristic(NimBLEUUID((uint16_t)0x2AD9));
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

// ═══════════════════════════════════════════════
// BLE Scanner — csak FTMS trainer keresése
// ═══════════════════════════════════════════════

static bool ftmsFound = false;
static bool scanRunning = false;

class ScanCallback : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        if (!ftmsFound && device->isAdvertisingService(NimBLEUUID((uint16_t)0x1826))) {
            Serial.printf("FTMS trainer found: %s (%s)\n",
                device->getName().c_str(),
                device->getAddress().toString().c_str());
            foundFtmsAddress = new NimBLEAddress(device->getAddress());
            ftmsFound = true;
            NimBLEDevice::getScan()->stop();
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        scanRunning = false;
        Serial.printf("Scan complete. FTMS found: %d\n", ftmsFound);
        NimBLEDevice::getAdvertising()->start();
    }
};

static ScanCallback scanCb;

bool bleScanStart() {
    if (scanRunning) return false;
    if (ftmsFound) return false;

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    if (scan->start(BLE_SCAN_DURATION_SEC, false, true)) {
        scanRunning = true;
        Serial.println("BLE scan started...");
        return true;
    }

    return false;
}

bool bleScanIsRunning() {
    return scanRunning;
}

bool bleScanFtmsFound() {
    return ftmsFound;
}

// ═══════════════════════════════════════════════
// Init
// ═══════════════════════════════════════════════

bool bleCentralInit() {
    NimBLEDevice::init(BRIDGE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    ftmsFound = false;
    scanRunning = false;

    return true;
}
