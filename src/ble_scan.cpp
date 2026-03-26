#include "ble_scan.h"
#include "config.h"
#include <NimBLEDevice.h>

// ───────────────────────────────────────────────
// Unified BLE Scanner
// Egyetlen nem-blokkoló scan, ami az összes szenzort
// egyszerre keresi UUID alapján.
// ───────────────────────────────────────────────

// Talált eszközök címei (más modulok olvassák)
NimBLEAddress* foundFtmsAddress = nullptr;
NimBLEAddress* foundHrmAddress = nullptr;
NimBLEAddress* foundCadenceAddress = nullptr;

static bool ftmsFound = false;
static bool hrmFound = false;
static bool cadenceFound = false;
static bool scanRunning = false;

// ───────────────────────────────────────────────
// Scan callback — mindhárom service-t figyeli
// ───────────────────────────────────────────────

class UnifiedScanCallback : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        // Debug: minden talált eszköz kiírása
        Serial.printf("  BLE device: %s (%s) RSSI:%d svcs:%d\n",
            device->getName().c_str(),
            device->getAddress().toString().c_str(),
            device->getRSSI(),
            device->getServiceUUIDCount());

        // FTMS trainer (0x1826)
        if (!ftmsFound && device->isAdvertisingService(NimBLEUUID((uint16_t)0x1826))) {
            Serial.printf("FTMS trainer found: %s (%s)\n",
                device->getName().c_str(),
                device->getAddress().toString().c_str());
            foundFtmsAddress = new NimBLEAddress(device->getAddress());
            ftmsFound = true;
        }

        // HRM szenzor (0x180D)
        if (!hrmFound && device->isAdvertisingService(NimBLEUUID((uint16_t)0x180D))) {
            Serial.printf("HRM sensor found: %s (%s)\n",
                device->getName().c_str(),
                device->getAddress().toString().c_str());
            foundHrmAddress = new NimBLEAddress(device->getAddress());
            hrmFound = true;
        }

        // Cadence szenzor (0x1816) — saját peripheral-t kiszűrjük
        if (!cadenceFound && device->isAdvertisingService(NimBLEUUID((uint16_t)0x1816))) {
            // Ne csatlakozzunk saját magunkhoz
            if (device->getName() != BRIDGE_DEVICE_NAME) {
                Serial.printf("Cadence sensor found: %s (%s)\n",
                    device->getName().c_str(),
                    device->getAddress().toString().c_str());
                foundCadenceAddress = new NimBLEAddress(device->getAddress());
                cadenceFound = true;
            }
        }

        // Ha mindent megtaláltunk, állítsuk le a scant
        if (ftmsFound && hrmFound && cadenceFound) {
            NimBLEDevice::getScan()->stop();
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        scanRunning = false;
        Serial.printf("Scan complete. Found: FTMS=%d HRM=%d CAD=%d\n",
            ftmsFound, hrmFound, cadenceFound);
    }
};

static UnifiedScanCallback unifiedScanCb;

// ───────────────────────────────────────────────
// Init
// ───────────────────────────────────────────────

void bleScanInit() {
    ftmsFound = false;
    hrmFound = false;
    cadenceFound = false;
    scanRunning = false;
}

// ───────────────────────────────────────────────
// Scan indítása (nem blokkoló!)
// ───────────────────────────────────────────────

bool bleScanStart() {
    if (scanRunning) return false;

    // Ha már mindent megtaláltunk, nem kell scanelni
    if (ftmsFound && hrmFound && cadenceFound) return false;

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&unifiedScanCb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    // Nem blokkoló: second param = false (not continue), third = true (async)
    if (scan->start(BLE_SCAN_DURATION_SEC, false, true)) {
        scanRunning = true;
        Serial.println("Unified BLE scan started...");
        return true;
    }

    return false;
}

// ───────────────────────────────────────────────
// Állapot
// ───────────────────────────────────────────────

bool bleScanIsRunning() {
    return scanRunning;
}

ScanResults bleScanGetResults() {
    return { ftmsFound, hrmFound, cadenceFound };
}
