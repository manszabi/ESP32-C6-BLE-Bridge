#include "ble_central_hrm.h"
#include "data_model.h"
#include "config.h"

#include <NimBLEDevice.h>

// ───────────────────────────────────────────────
// HRM Central modul
// Service: 0x180D (Heart Rate)
// Characteristic: 0x2A37 (Heart Rate Measurement, notify)
//
// Heart Rate Measurement frame:
// [0]    Flags (bit0: 0=uint8 HR, 1=uint16 HR)
// [1]    Heart Rate (uint8) — ha bit0=0
// [1-2]  Heart Rate (uint16) — ha bit0=1
// ───────────────────────────────────────────────

static NimBLEClient* hrmClient = nullptr;
static NimBLERemoteCharacteristic* hrmMeasChar = nullptr;
static NimBLEAddress* hrmAddress = nullptr;
static bool hrmFound = false;

// ───────────────────────────────────────────────
// HRM Notify callback
// ───────────────────────────────────────────────

static void onHrmNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    if (len < 2) return;

    uint8_t flags = data[0];
    int heartRate = 0;

    // Bit 0: Heart Rate Value Format
    // 0 = uint8, 1 = uint16
    if (flags & 0x01) {
        // uint16 format
        if (len >= 3) {
            heartRate = (int)(data[1] | (data[2] << 8));
        }
    } else {
        // uint8 format
        heartRate = (int)data[1];
    }

    if (heartRate > 0 && heartRate < 250) {
        updateHeartRate(heartRate);
    }
}

// ───────────────────────────────────────────────
// Scan callback — HRM szenzor keresése
// ───────────────────────────────────────────────

class HrmScanCallback : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        if (device->isAdvertisingService(NimBLEUUID("180D"))) {
            Serial.printf("HRM sensor found: %s (%s)\n",
                device->getName().c_str(),
                device->getAddress().toString().c_str());

            hrmAddress = new NimBLEAddress(device->getAddress());
            hrmFound = true;

            NimBLEDevice::getScan()->stop();
        }
    }
};

static HrmScanCallback hrmScanCb;

// ───────────────────────────────────────────────
// Init
// ───────────────────────────────────────────────

bool bleCentralHrmInit() {
    hrmFound = false;
    return true;
}

// ───────────────────────────────────────────────
// Connect — scan + csatlakozás
// ───────────────────────────────────────────────

bool bleCentralHrmConnect() {
    // Ha már csatlakozva van, ne csináljon semmit
    if (hrmClient && hrmClient->isConnected()) {
        return true;
    }

    // Ha még nem találtuk meg a szenzort, scan indítása
    if (!hrmFound) {
        Serial.println("Scanning for HRM sensor...");

        NimBLEScan* scan = NimBLEDevice::getScan();
        scan->setScanCallbacks(&hrmScanCb, false);
        scan->setActiveScan(true);
        scan->setInterval(100);
        scan->setWindow(99);
        scan->start(BLE_SCAN_DURATION_SEC, false);

        if (!hrmFound) {
            Serial.println("HRM sensor not found during scan.");
            return false;
        }
    }

    // Csatlakozás
    hrmClient = NimBLEDevice::createClient();

    Serial.println("Connecting to HRM sensor...");
    if (!hrmClient->connect(*hrmAddress)) {
        Serial.println("Failed to connect to HRM sensor!");
        return false;
    }

    Serial.println("Connected to HRM, discovering services...");

    // Heart Rate Service (0x180D)
    NimBLERemoteService* hrmService = hrmClient->getService("180D");
    if (!hrmService) {
        Serial.println("Heart Rate service not found!");
        hrmClient->disconnect();
        return false;
    }

    // Heart Rate Measurement (0x2A37)
    hrmMeasChar = hrmService->getCharacteristic("2A37");
    if (!hrmMeasChar) {
        Serial.println("Heart Rate Measurement characteristic not found!");
        hrmClient->disconnect();
        return false;
    }

    // Subscribe notify-ra
    if (hrmMeasChar->canNotify()) {
        hrmMeasChar->subscribe(true, onHrmNotify);
        Serial.println("Subscribed to Heart Rate Measurement notifications.");
    } else {
        Serial.println("HRM characteristic cannot notify!");
        hrmClient->disconnect();
        return false;
    }

    return true;
}

// ───────────────────────────────────────────────
// Loop — NimBLE callback-alapú, nincs teendő
// ───────────────────────────────────────────────

void bleCentralHrmLoop() {
    // callback-alapú, a notify automatikusan jön
}

// ───────────────────────────────────────────────
// Állapot lekérdezés
// ───────────────────────────────────────────────

bool bleCentralHrmIsConnected() {
    return hrmClient && hrmClient->isConnected();
}