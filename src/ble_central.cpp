#include "ble_central.h"
#include "config.h"
#include "data_model.h"

#include <NimBLEDevice.h>

// ═══════════════════════════════════════════════
// FTMS Parser (Indoor Bike Data)
// ═══════════════════════════════════════════════

#define FLAG_MORE_DATA           (1u << 0)   // invertált! 0 = speed jelen van
#define FLAG_AVG_SPEED           (1u << 1)
#define FLAG_INST_CADENCE        (1u << 2)
#define FLAG_AVG_CADENCE         (1u << 3)
#define FLAG_TOTAL_DISTANCE      (1u << 4)
#define FLAG_RESISTANCE_LEVEL    (1u << 5)
#define FLAG_INST_POWER          (1u << 6)
#define FLAG_AVG_POWER           (1u << 7)
#define FLAG_EXPENDED_ENERGY     (1u << 8)
#define FLAG_HEART_RATE          (1u << 9)
#define FLAG_METABOLIC_EQUIV     (1u << 10)
#define FLAG_ELAPSED_TIME        (1u << 11)
#define FLAG_REMAINING_TIME      (1u << 12)

static inline uint16_t readU16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline int16_t readS16(const uint8_t* p) {
    return (int16_t)(p[0] | (p[1] << 8));
}

static void parseFtmsIndoorBikeData(const uint8_t* data, size_t len) {
    if (len < 2) return;

    size_t offset = 0;
    uint16_t flags = readU16(&data[offset]);
    offset += 2;

    float speed = 0.0f;
    int cadence = 0;
    int power = 0;

    // Bit 0 (invertált): Instantaneous Speed (km/h, 0.01 felbontás)
    if (!(flags & FLAG_MORE_DATA)) {
        if (offset + 2 > len) return;
        speed = readU16(&data[offset]) / 100.0f;
        offset += 2;
    }

    // Bit 1: Average Speed
    if (flags & FLAG_AVG_SPEED) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // Bit 2: Instantaneous Cadence (rpm, 0.5 felbontás)
    if (flags & FLAG_INST_CADENCE) {
        if (offset + 2 > len) return;
        cadence = readU16(&data[offset]) / 2;
        offset += 2;
    }

    // Bit 3: Average Cadence
    if (flags & FLAG_AVG_CADENCE) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // Bit 4: Total Distance (3 byte)
    if (flags & FLAG_TOTAL_DISTANCE) {
        if (offset + 3 > len) return;
        offset += 3;
    }

    // Bit 5: Resistance Level
    if (flags & FLAG_RESISTANCE_LEVEL) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // Bit 6: Instantaneous Power (watt)
    if (flags & FLAG_INST_POWER) {
        if (offset + 2 > len) return;
        power = readS16(&data[offset]);
        offset += 2;
    }

    // Bit 7: Average Power
    if (flags & FLAG_AVG_POWER) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // Bit 8: Expended Energy (5 byte)
    if (flags & FLAG_EXPENDED_ENERGY) {
        if (offset + 5 > len) return;
        offset += 5;
    }

    // Bit 9: Heart Rate
    if (flags & FLAG_HEART_RATE) {
        if (offset + 1 > len) return;
        offset += 1;
    }

    // Bit 10: Metabolic Equivalent
    if (flags & FLAG_METABOLIC_EQUIV) {
        if (offset + 1 > len) return;
        offset += 1;
    }

    // Bit 11: Elapsed Time
    if (flags & FLAG_ELAPSED_TIME) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // Bit 12: Remaining Time
    if (flags & FLAG_REMAINING_TIME) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    updateTrainerFromSuito(power, cadence, speed);
}

// ═══════════════════════════════════════════════
// Suito Central (FTMS)
// ═══════════════════════════════════════════════

static NimBLEClient* suitoClient = nullptr;
static NimBLERemoteCharacteristic* ftmsIndoorBikeDataChar = nullptr;
static NimBLERemoteCharacteristic* ftmsControlPointChar   = nullptr;

static void onFtmsNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    parseFtmsIndoorBikeData(data, len);
}

bool bleCentralConnectToSuito() {
    if (suitoClient && suitoClient->isConnected()) {
        return true;
    }

    extern NimBLEAddress* foundFtmsAddress;
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
// HRM Central
// ═══════════════════════════════════════════════

static NimBLEClient* hrmClient = nullptr;
static NimBLERemoteCharacteristic* hrmMeasChar = nullptr;

static void onHrmNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    if (len < 2) return;

    uint8_t flags = data[0];
    int heartRate = 0;

    if (flags & 0x01) {
        if (len >= 3) {
            heartRate = (int)(data[1] | (data[2] << 8));
        }
    } else {
        heartRate = (int)data[1];
    }

    if (heartRate > 0 && heartRate < 250) {
        updateHeartRate(heartRate);
    }
}

bool bleCentralHrmConnect() {
    if (hrmClient && hrmClient->isConnected()) {
        return true;
    }

    extern NimBLEAddress* foundHrmAddress;
    if (!foundHrmAddress) {
        return false;
    }

    if (!hrmClient) {
        hrmClient = NimBLEDevice::createClient();
        if (!hrmClient) {
            Serial.println("Failed to create BLE client for HRM!");
            return false;
        }
    }

    Serial.println("Connecting to HRM sensor...");
    if (!hrmClient->connect(*foundHrmAddress)) {
        Serial.println("Failed to connect to HRM sensor!");
        return false;
    }

    Serial.println("Connected to HRM, discovering services...");

    NimBLERemoteService* hrmService = hrmClient->getService(NimBLEUUID((uint16_t)0x180D));
    if (!hrmService) {
        Serial.println("Heart Rate service not found!");
        hrmClient->disconnect();
        return false;
    }

    hrmMeasChar = hrmService->getCharacteristic(NimBLEUUID((uint16_t)0x2A37));
    if (!hrmMeasChar) {
        Serial.println("Heart Rate Measurement characteristic not found!");
        hrmClient->disconnect();
        return false;
    }

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

bool bleCentralHrmIsConnected() {
    return hrmClient && hrmClient->isConnected();
}

// ═══════════════════════════════════════════════
// Cadence Central (CSC)
// ═══════════════════════════════════════════════

static NimBLEClient* cadClient = nullptr;
static NimBLERemoteCharacteristic* cscMeasCharCentral = nullptr;

static uint16_t prevCrankRevs = 0;
static uint16_t prevCrankTime = 0;
static bool firstCrankData = true;

static void onCscNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    if (len < 1) return;

    uint8_t flags = data[0];
    size_t offset = 1;

    // Bit 0: Wheel Revolution Data present
    if (flags & 0x01) {
        if (offset + 6 > len) return;
        offset += 6;
    }

    // Bit 1: Crank Revolution Data present
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

        uint16_t deltaRevs = crankRevs - prevCrankRevs;
        uint16_t deltaTime = crankTime - prevCrankTime;

        if (deltaTime > 0 && deltaRevs > 0 && deltaRevs < 20) {
            float cadence = (deltaRevs * 60.0f * 1024.0f) / deltaTime;

            if (cadence > 0 && cadence < 250) {
                updateExternalCadence((int)(cadence + 0.5f));
            }
        }

        prevCrankRevs = crankRevs;
        prevCrankTime = crankTime;
    }
}

bool bleCentralCadenceConnect() {
    if (cadClient && cadClient->isConnected()) {
        return true;
    }

    extern NimBLEAddress* foundCadenceAddress;
    if (!foundCadenceAddress) {
        return false;
    }

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

    cscMeasCharCentral = cscService->getCharacteristic(NimBLEUUID((uint16_t)0x2A5B));
    if (!cscMeasCharCentral) {
        Serial.println("CSC Measurement characteristic not found!");
        cadClient->disconnect();
        return false;
    }

    if (cscMeasCharCentral->canNotify()) {
        cscMeasCharCentral->subscribe(true, onCscNotify);
        Serial.println("Subscribed to CSC Measurement notifications.");
    } else {
        Serial.println("CSC Measurement cannot notify!");
        cadClient->disconnect();
        return false;
    }

    firstCrankData = true;
    return true;
}

bool bleCentralCadenceIsConnected() {
    return cadClient && cadClient->isConnected();
}

// ═══════════════════════════════════════════════
// Unified BLE Scanner
// ═══════════════════════════════════════════════

NimBLEAddress* foundFtmsAddress = nullptr;
NimBLEAddress* foundHrmAddress = nullptr;
NimBLEAddress* foundCadenceAddress = nullptr;

static bool ftmsFound = false;
static bool hrmFound = false;
static bool cadenceFound = false;
static bool scanRunning = false;

class UnifiedScanCallback : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        if (!ftmsFound && device->isAdvertisingService(NimBLEUUID((uint16_t)0x1826))) {
            Serial.printf("FTMS trainer found: %s (%s)\n",
                device->getName().c_str(),
                device->getAddress().toString().c_str());
            foundFtmsAddress = new NimBLEAddress(device->getAddress());
            ftmsFound = true;
        }

        if (!hrmFound && device->isAdvertisingService(NimBLEUUID((uint16_t)0x180D))) {
            Serial.printf("HRM sensor found: %s (%s)\n",
                device->getName().c_str(),
                device->getAddress().toString().c_str());
            foundHrmAddress = new NimBLEAddress(device->getAddress());
            hrmFound = true;
        }

        if (!cadenceFound && device->isAdvertisingService(NimBLEUUID((uint16_t)0x1816))) {
            if (device->getName() != BRIDGE_DEVICE_NAME) {
                Serial.printf("Cadence sensor found: %s (%s)\n",
                    device->getName().c_str(),
                    device->getAddress().toString().c_str());
                foundCadenceAddress = new NimBLEAddress(device->getAddress());
                cadenceFound = true;
            }
        }

        if (ftmsFound && hrmFound && cadenceFound) {
            NimBLEDevice::getScan()->stop();
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        scanRunning = false;
        Serial.printf("Scan complete. Found: FTMS=%d HRM=%d CAD=%d\n",
            ftmsFound, hrmFound, cadenceFound);
        NimBLEDevice::getAdvertising()->start();
    }
};

static UnifiedScanCallback unifiedScanCb;

bool bleScanStart() {
    if (scanRunning) return false;
    if (ftmsFound && hrmFound && cadenceFound) return false;

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&unifiedScanCb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    if (scan->start(BLE_SCAN_DURATION_SEC, false, true)) {
        scanRunning = true;
        Serial.println("Unified BLE scan started...");
        return true;
    }

    return false;
}

bool bleScanIsRunning() {
    return scanRunning;
}

ScanResults bleScanGetResults() {
    return { ftmsFound, hrmFound, cadenceFound };
}

// ═══════════════════════════════════════════════
// Init (NimBLE + scan setup)
// ═══════════════════════════════════════════════

bool bleCentralInit() {
    NimBLEDevice::init(BRIDGE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    ftmsFound = false;
    hrmFound = false;
    cadenceFound = false;
    scanRunning = false;
    firstCrankData = true;

    return true;
}
