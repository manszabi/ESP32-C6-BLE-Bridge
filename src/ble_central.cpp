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

static NimBLEAddress foundFtmsAddress;
static bool hasFoundAddress = false;
static volatile bool disconnectedFlag = false;

// ───────────────────────────────────────────────
// Client callbacks — onDisconnect azonnali észlelése
// ───────────────────────────────────────────────

class SuitoClientCallbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient* client, int reason) override {
        disconnectedFlag = true;
        ftmsIndoorBikeDataChar = nullptr;
        ftmsControlPointChar = nullptr;
#if DEBUG_SERIAL
        Serial.printf("[CONN] Suito disconnected (reason: %d)\n", reason);
#endif
    }
};

static SuitoClientCallbacks suitoClientCb;

static void onFtmsNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    parseFtmsIndoorBikeData(data, len);
}

bool bleCentralConnectToSuito() {
    if (suitoClient && suitoClient->isConnected()) {
        return true;
    }

    if (!hasFoundAddress) {
        return false;
    }

    if (!suitoClient) {
        suitoClient = NimBLEDevice::createClient();
        if (!suitoClient) {
            Serial.println("[CONN] Failed to create BLE client!");
            return false;
        }
        suitoClient->setClientCallbacks(&suitoClientCb, false);
    }

    Serial.println("[CONN] Connecting to FTMS trainer...");
    if (!suitoClient->connect(foundFtmsAddress)) {
        Serial.println("[CONN] Failed to connect to FTMS trainer!");
        return false;
    }

    Serial.println("[CONN] Connected, discovering FTMS services...");
    disconnectedFlag = false;

    NimBLERemoteService* ftmsService = suitoClient->getService(NimBLEUUID((uint16_t)0x1826));
    if (!ftmsService) {
        Serial.println("[CONN] FTMS service not found!");
        suitoClient->disconnect();
        return false;
    }

    ftmsIndoorBikeDataChar = ftmsService->getCharacteristic(NimBLEUUID((uint16_t)0x2AD2));
    if (!ftmsIndoorBikeDataChar) {
        Serial.println("[CONN] Indoor Bike Data characteristic not found!");
        suitoClient->disconnect();
        return false;
    }

    ftmsControlPointChar = ftmsService->getCharacteristic(NimBLEUUID((uint16_t)0x2AD9));
    if (!ftmsControlPointChar) {
        Serial.println("[CONN] Control Point not found (non-fatal).");
    }

    if (ftmsIndoorBikeDataChar->canNotify()) {
        ftmsIndoorBikeDataChar->subscribe(true, onFtmsNotify);
        Serial.println("[CONN] Subscribed to Indoor Bike Data.");
    } else {
        Serial.println("[CONN] Indoor Bike Data cannot notify!");
        suitoClient->disconnect();
        return false;
    }

    return true;
}

bool bleCentralIsConnected() {
    return suitoClient && suitoClient->isConnected();
}

bool bleCentralWasDisconnected() {
    if (disconnectedFlag) {
        disconnectedFlag = false;
        return true;
    }
    return false;
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
// BLE Scanner — FTMS trainer keresése
// ═══════════════════════════════════════════════

static bool ftmsFound = false;
static bool scanRunning = false;

class ScanCallback : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        if (!ftmsFound && device->isAdvertisingService(NimBLEUUID((uint16_t)0x1826))) {
            Serial.printf("[SCAN] FTMS trainer found: %s (%s)\n",
                device->getName().c_str(),
                device->getAddress().toString().c_str());
            foundFtmsAddress = device->getAddress();
            hasFoundAddress = true;
            ftmsFound = true;
            NimBLEDevice::getScan()->stop();
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        scanRunning = false;
#if DEBUG_SERIAL
        Serial.printf("[SCAN] Complete. FTMS found: %d\n", ftmsFound);
#endif
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
    scan->setDuplicateFilter(true);

    if (scan->start(BLE_SCAN_DURATION_SEC, false, true)) {
        scanRunning = true;
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

void bleScanReset() {
    ftmsFound = false;
    hasFoundAddress = false;
#if DEBUG_SERIAL
    Serial.println("[SCAN] Reset — will search for trainer again");
#endif
}

// ═══════════════════════════════════════════════
// Init
// ═══════════════════════════════════════════════

bool bleCentralInit() {
    NimBLEDevice::init(BRIDGE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setScanDuplicateCacheSize(20);

    ftmsFound = false;
    hasFoundAddress = false;
    scanRunning = false;
    disconnectedFlag = false;

    return true;
}
