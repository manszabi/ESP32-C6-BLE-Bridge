#include "ble_central_cadence.h"
#include "data_model.h"
#include "config.h"

#include <NimBLEDevice.h>

// ───────────────────────────────────────────────
// Cadence Central modul
// Service: 0x1816 (Cycling Speed and Cadence)
// Characteristic: 0x2A5B (CSC Measurement, notify)
//
// CSC Measurement frame:
// [0]    Flags (bit0: Wheel data, bit1: Crank data)
// Ha bit0=1:
//   [1-4]  Cumulative Wheel Revolutions (uint32)
//   [5-6]  Last Wheel Event Time (uint16, 1/1024 sec)
// Ha bit1=1:
//   [next]   Cumulative Crank Revolutions (uint16)
//   [next+2] Last Crank Event Time (uint16, 1/1024 sec)
// ───────────────────────────────────────────────

static NimBLEClient* cadClient = nullptr;
static NimBLERemoteCharacteristic* cscMeasChar = nullptr;
static NimBLEAddress* cadAddress = nullptr;
static bool cadFound = false;

// Előző crank értékek a cadence számoláshoz
static uint16_t prevCrankRevs = 0;
static uint16_t prevCrankTime = 0;
static bool firstCrankData = true;

// ───────────────────────────────────────────────
// CSC Notify callback
// ───────────────────────────────────────────────

static void onCscNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    if (len < 1) return;

    uint8_t flags = data[0];
    size_t offset = 1;

    // Bit 0: Wheel Revolution Data present — átlépjük
    if (flags & 0x01) {
        if (offset + 6 > len) return;
        // uint32 wheel revs + uint16 last wheel event time
        offset += 6;
    }

    // Bit 1: Crank Revolution Data present — ez kell nekünk
    if (flags & 0x02) {
        if (offset + 4 > len) return;

        uint16_t crankRevs = (uint16_t)(data[offset] | (data[offset + 1] << 8));
        uint16_t crankTime = (uint16_t)(data[offset + 2] | (data[offset + 3] << 8));

        if (firstCrankData) {
            // Első adat: csak eltároljuk, nem számolunk cadence-t
            prevCrankRevs = crankRevs;
            prevCrankTime = crankTime;
            firstCrankData = false;
            return;
        }

        // Delta számítás (uint16 overflow-safe)
        uint16_t deltaRevs = crankRevs - prevCrankRevs;
        uint16_t deltaTime = crankTime - prevCrankTime;

        if (deltaTime > 0 && deltaRevs > 0 && deltaRevs < 20) {
            // deltaTime 1/1024 sec egységben
            // cadence = (deltaRevs / deltaTime) * 60 * 1024
            float cadence = (deltaRevs * 60.0f * 1024.0f) / deltaTime;

            if (cadence > 0 && cadence < 250) {
                updateExternalCadence((int)(cadence + 0.5f));
            }
        }

        prevCrankRevs = crankRevs;
        prevCrankTime = crankTime;
    }
}

// ───────────────────────────────────────────────
// Scan callback — Cadence szenzor keresése
// ───────────────────────────────────────────────

class CadenceScanCallback : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        if (device->isAdvertisingService(NimBLEUUID("1816"))) {
            Serial.printf("Cadence sensor found: %s (%s)\n",
                device->getName().c_str(),
                device->getAddress().toString().c_str());

            cadAddress = new NimBLEAddress(device->getAddress());
            cadFound = true;

            NimBLEDevice::getScan()->stop();
        }
    }
};

static CadenceScanCallback cadScanCb;

// ───────────────────────────────────────────────
// Init
// ───────────────────────────────────────────────

bool bleCentralCadenceInit() {
    cadFound = false;
    firstCrankData = true;
    return true;
}

// ───────────────────────────────────────────────
// Connect — scan + csatlakozás
// ───────────────────────────────────────────────

bool bleCentralCadenceConnect() {
    if (cadClient && cadClient->isConnected()) {
        return true;
    }

    // Scan a cadence szenzorra
    if (!cadFound) {
        Serial.println("Scanning for Cadence sensor...");

        NimBLEScan* scan = NimBLEDevice::getScan();
        scan->setScanCallbacks(&cadScanCb, false);
        scan->setActiveScan(true);
        scan->setInterval(100);
        scan->setWindow(99);
        scan->start(BLE_SCAN_DURATION_SEC, false);

        if (!cadFound) {
            Serial.println("Cadence sensor not found during scan.");
            return false;
        }
    }

    cadClient = NimBLEDevice::createClient();

    Serial.println("Connecting to Cadence sensor...");
    if (!cadClient->connect(*cadAddress)) {
        Serial.println("Failed to connect to Cadence sensor!");
        return false;
    }

    Serial.println("Connected to Cadence sensor, discovering services...");

    NimBLERemoteService* cscService = cadClient->getService("1816");
    if (!cscService) {
        Serial.println("CSC service not found!");
        cadClient->disconnect();
        return false;
    }

    cscMeasChar = cscService->getCharacteristic("2A5B");
    if (!cscMeasChar) {
        Serial.println("CSC Measurement characteristic not found!");
        cadClient->disconnect();
        return false;
    }

    if (cscMeasChar->canNotify()) {
        cscMeasChar->subscribe(true, onCscNotify);
        Serial.println("Subscribed to CSC Measurement notifications.");
    } else {
        Serial.println("CSC Measurement cannot notify!");
        cadClient->disconnect();
        return false;
    }

    firstCrankData = true;
    return true;
}

// ───────────────────────────────────────────────
// Loop
// ───────────────────────────────────────────────

void bleCentralCadenceLoop() {
    // callback-alapú
}

// ───────────────────────────────────────────────
// Állapot lekérdezés
// ───────────────────────────────────────────────

bool bleCentralCadenceIsConnected() {
    return cadClient && cadClient->isConnected();
}