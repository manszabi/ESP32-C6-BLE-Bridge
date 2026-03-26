#include "ble_central_cadence.h"
#include "data_model.h"
#include "config.h"
#include "ble_scan.h"

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

// Cím a unified scannerből
extern NimBLEAddress* foundCadenceAddress;

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
        offset += 6;
    }

    // Bit 1: Crank Revolution Data present — ez kell nekünk
    if (flags & 0x02) {
        if (offset + 4 > len) return;

        uint16_t crankRevs = (uint16_t)(data[offset] | (data[offset + 1] << 8));
        uint16_t crankTime = (uint16_t)(data[offset + 2] | (data[offset + 3] << 8));

        if (firstCrankData) {
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
// Init
// ───────────────────────────────────────────────

bool bleCentralCadenceInit() {
    firstCrankData = true;
    return true;
}

// ───────────────────────────────────────────────
// Connect — unified scanner-ből kapott címmel
// ───────────────────────────────────────────────

bool bleCentralCadenceConnect() {
    if (cadClient && cadClient->isConnected()) {
        return true;
    }

    // Várjuk meg, amíg a unified scanner megtalálja
    if (!foundCadenceAddress) {
        return false;
    }

    // Klienst csak egyszer hozzuk létre
    if (!cadClient) {
        cadClient = NimBLEDevice::createClient();
        if (!cadClient) {
            Serial.println("Failed to create BLE client for Cadence!");
            return false;
        }
    }

    Serial.println("Connecting to Cadence sensor...");
    if (!cadClient->connect(*foundCadenceAddress)) {
        Serial.println("Failed to connect to Cadence sensor!");
        return false;
    }

    Serial.println("Connected to Cadence sensor, discovering services...");

    NimBLERemoteService* cscService = cadClient->getService(NimBLEUUID((uint16_t)0x1816));
    if (!cscService) {
        Serial.println("CSC service not found!");
        cadClient->disconnect();
        return false;
    }

    cscMeasChar = cscService->getCharacteristic(NimBLEUUID((uint16_t)0x2A5B));
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
}

// ───────────────────────────────────────────────
// Állapot lekérdezés
// ───────────────────────────────────────────────

bool bleCentralCadenceIsConnected() {
    return cadClient && cadClient->isConnected();
}
