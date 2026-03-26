#include "ble_peripheral.h"
#include "ble_central.h"
#include "data_model.h"
#include "config.h"

#include <NimBLEDevice.h>

// ───────────────────────────────────────────────
// BLE server + service/characteristic pointerek
// ───────────────────────────────────────────────

static NimBLEServer* bleServer = nullptr;

// FTMS (Zwift)
static NimBLEService* ftmsService = nullptr;
static NimBLECharacteristic* ftmsDataChar = nullptr;
static NimBLECharacteristic* ftmsControlPointChar = nullptr;
static NimBLECharacteristic* ftmsFeatureChar = nullptr;

// CPS (Garmin)
static NimBLEService* cpsService = nullptr;
static NimBLECharacteristic* cpsPowerChar = nullptr;
static NimBLECharacteristic* cpsFeatureChar = nullptr;
static NimBLECharacteristic* cpsSensorLocChar = nullptr;

// CSC (Telefon)
static NimBLEService* cscService = nullptr;
static NimBLECharacteristic* cscMeasureChar = nullptr;
static NimBLECharacteristic* cscFeatureChar = nullptr;

// ───────────────────────────────────────────────
// Kumulatív számlálók (CPS és CSC-hez kellenek)
// ───────────────────────────────────────────────

static uint16_t cumulativeCrankRevs = 0;
static uint16_t lastCrankEventTime  = 0;   // 1/1024 sec egységben

static uint32_t cumulativeWheelRevs = 0;
static uint16_t lastWheelEventTime  = 0;   // 1/1024 sec egységben

static uint32_t lastCrankUpdateMs = 0;
static uint32_t lastWheelUpdateMs = 0;

// ───────────────────────────────────────────────
// Kliens-ellenőrzés
// ───────────────────────────────────────────────

bool isFtmsClientConnected() {
    return bleServer && bleServer->getConnectedCount() > 0 && ftmsDataChar;
}

bool isCpsClientConnected() {
    return bleServer && bleServer->getConnectedCount() > 0 && cpsPowerChar;
}

bool isCscClientConnected() {
    return bleServer && bleServer->getConnectedCount() > 0 && cscMeasureChar;
}

// ───────────────────────────────────────────────
// Zwift → ESP32 → Suito vezérlés (Control Point)
// ───────────────────────────────────────────────

class FtmsControlPointCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
        NimBLEAttValue val = c->getValue();
        if (val.size() < 3) return;

        const uint8_t* d = val.data();
        uint8_t opcode = d[0];

        // FTMS Control Point opcode-ok:
        // 0x05 = Set Target Power (sint16, watts)
        // 0x11 = Set Indoor Bike Simulation (wind, grade, crr, cw)
        if (opcode == 0x05 && val.size() >= 3) {
            int16_t targetPower = (int16_t)(d[1] | (d[2] << 8));
            handleControlFromClient(targetPower);
        }
        // TODO: 0x11 simulation params kezelése
    }
};

// ───────────────────────────────────────────────
// BLE Peripheral inicializálás
// ───────────────────────────────────────────────

bool blePeripheralInit() {
    NimBLEDevice::init(BRIDGE_DEVICE_NAME);
    bleServer = NimBLEDevice::createServer();

    // ─────────────────────────────────────────
    // 1. FTMS Service (0x1826) — Zwift
    // ─────────────────────────────────────────
    ftmsService = bleServer->createService("1826");

    // Fitness Machine Feature (0x2ACC, read)
    // Byte layout: 4 byte Fitness Machine Features + 4 byte Target Setting Features
    ftmsFeatureChar = ftmsService->createCharacteristic(
        "2ACC",
        NIMBLE_PROPERTY::READ
    );
    // Bit 1: Cadence Supported
    // Bit 2: Total Distance Supported
    // Bit 6: Power Measurement Supported
    // Bit 14: Indoor Bike Simulation Parameters Supported (Target Settings)
    uint8_t ftmsFeature[8] = {0};
    ftmsFeature[0] = 0x46;  // bits 1,2,6 = cadence + distance + power
    ftmsFeature[1] = 0x00;
    ftmsFeature[4] = 0x00;
    ftmsFeature[5] = 0x40;  // bit 14 = indoor bike simulation
    ftmsFeatureChar->setValue(ftmsFeature, sizeof(ftmsFeature));

    // Indoor Bike Data (0x2AD2, notify)
    ftmsDataChar = ftmsService->createCharacteristic(
        "2AD2",
        NIMBLE_PROPERTY::NOTIFY
    );

    // Fitness Machine Control Point (0x2AD9, write + indicate)
    ftmsControlPointChar = ftmsService->createCharacteristic(
        "2AD9",
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE
    );
    ftmsControlPointChar->setCallbacks(new FtmsControlPointCallback());

    // ─────────────────────────────────────────
    // 2. CPS Service (0x1818) — Garmin
    // ─────────────────────────────────────────
    cpsService = bleServer->createService("1818");

    // Cycling Power Feature (0x2A65, read)
    cpsFeatureChar = cpsService->createCharacteristic(
        "2A65",
        NIMBLE_PROPERTY::READ
    );
    // Bit 3: Crank Revolution Data Supported
    uint8_t cpsFeature[4] = {0};
    cpsFeature[0] = 0x08;  // bit 3
    cpsFeatureChar->setValue(cpsFeature, sizeof(cpsFeature));

    // Sensor Location (0x2A5D, read)
    cpsSensorLocChar = cpsService->createCharacteristic(
        "2A5D",
        NIMBLE_PROPERTY::READ
    );
    uint8_t sensorLoc = 0x0D;  // "Rear Hub"
    cpsSensorLocChar->setValue(&sensorLoc, 1);

    // Cycling Power Measurement (0x2A63, notify)
    cpsPowerChar = cpsService->createCharacteristic(
        "2A63",
        NIMBLE_PROPERTY::NOTIFY
    );

    // ─────────────────────────────────────────
    // 3. CSC Service (0x1816) — Telefon
    // ─────────────────────────────────────────
    cscService = bleServer->createService("1816");

    // CSC Feature (0x2A5C, read)
    cscFeatureChar = cscService->createCharacteristic(
        "2A5C",
        NIMBLE_PROPERTY::READ
    );
    // Bit 0: Wheel Revolution Data Supported
    // Bit 1: Crank Revolution Data Supported
    uint8_t cscFeature[2] = {0};
    cscFeature[0] = 0x03;  // bit 0 + bit 1
    cscFeatureChar->setValue(cscFeature, sizeof(cscFeature));

    // CSC Measurement (0x2A5B, notify)
    cscMeasureChar = cscService->createCharacteristic(
        "2A5B",
        NIMBLE_PROPERTY::NOTIFY
    );

    return true;
}

// ───────────────────────────────────────────────
// Advertising indítása
// ───────────────────────────────────────────────

void blePeripheralStartAdvertising() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();

    adv->addServiceUUID("1826");  // FTMS
    adv->addServiceUUID("1818");  // CPS
    adv->addServiceUUID("1816");  // CSC

    adv->start();
    Serial.println("BLE advertising started.");
}

// ───────────────────────────────────────────────
// Segédfüggvény: crank/wheel számlálók frissítése
// ───────────────────────────────────────────────

static void updateCrankCounter() {
    uint32_t now = millis();
    int cadence = g_trainerData.cadence;

    if (cadence > 0 && now > lastCrankUpdateMs) {
        uint32_t elapsedMs = now - lastCrankUpdateMs;

        // Hány fordulat történt az eltelt idő alatt?
        // cadence = rpm → rev/sec = cadence/60
        // revolutions = (cadence/60) * (elapsedMs/1000)
        float revs = (cadence / 60.0f) * (elapsedMs / 1000.0f);

        if (revs >= 0.1f) {
            cumulativeCrankRevs += (uint16_t)(revs + 0.5f);  // kerekítés
            // Last Crank Event Time: 1/1024 sec egységben, 16 bit (overflowol ~64s-enként)
            lastCrankEventTime += (uint16_t)((elapsedMs * 1024) / 1000);
            lastCrankUpdateMs = now;
        }
    } else {
        lastCrankUpdateMs = now;
    }
}

static void updateWheelCounter() {
    uint32_t now = millis();
    float speed = g_trainerData.speed;  // km/h

    if (speed > 0.1f && now > lastWheelUpdateMs) {
        uint32_t elapsedMs = now - lastWheelUpdateMs;

        // speed km/h → m/s = speed / 3.6
        // wheel circumference = 2.105m (700x25c)
        // revolutions = (m/s * elapsedMs/1000) / circumference
        float distM = (speed / 3.6f) * (elapsedMs / 1000.0f);
        float revs = distM / 2.105f;

        if (revs >= 0.1f) {
            cumulativeWheelRevs += (uint32_t)(revs + 0.5f);
            lastWheelEventTime += (uint16_t)((elapsedMs * 1024) / 1000);
            lastWheelUpdateMs = now;
        }
    } else {
        lastWheelUpdateMs = now;
    }
}

// ───────────────────────────────────────────────
// FTMS Indoor Bike Data notify (Zwift)
//
// Frame formátum (spec 4.9.1):
// [0-1]  Flags (16 bit, little-endian)
// [2-3]  Instantaneous Speed (uint16, 0.01 km/h) — ha bit0=0
// [4-5]  Instantaneous Cadence (uint16, 0.5 rpm) — ha bit2=1
// [6-7]  Instantaneous Power (sint16, watt) — ha bit6=1
// ───────────────────────────────────────────────

void blePeripheralSendFtms() {
    if (!ftmsDataChar) return;

    uint8_t frame[8] = {0};
    size_t offset = 0;

    // Flags: bit0=0 (speed present), bit2=1 (cadence), bit6=1 (power)
    uint16_t flags = 0x0044;  // 0b0000_0000_0100_0100
    frame[offset++] = flags & 0xFF;
    frame[offset++] = (flags >> 8) & 0xFF;

    // Instantaneous Speed (bit0=0 → jelen van)
    uint16_t rawSpeed = (uint16_t)(g_trainerData.speed * 100);  // 0.01 km/h
    frame[offset++] = rawSpeed & 0xFF;
    frame[offset++] = (rawSpeed >> 8) & 0xFF;

    // Instantaneous Cadence (bit2=1 → jelen van)
    uint16_t rawCadence = (uint16_t)(g_trainerData.cadence * 2);  // 0.5 rpm
    frame[offset++] = rawCadence & 0xFF;
    frame[offset++] = (rawCadence >> 8) & 0xFF;

    // Instantaneous Power (bit6=1 → jelen van)
    int16_t rawPower = (int16_t)g_trainerData.power;
    frame[offset++] = rawPower & 0xFF;
    frame[offset++] = (rawPower >> 8) & 0xFF;

    ftmsDataChar->setValue(frame, offset);
    ftmsDataChar->notify();
}

// ───────────────────────────────────────────────
// CPS Cycling Power Measurement notify (Garmin)
//
// Frame formátum:
// [0-1]  Flags (16 bit) — bit5=1: Crank Revolution Data present
// [2-3]  Instantaneous Power (sint16, watt)
// [4-5]  Cumulative Crank Revolutions (uint16)
// [6-7]  Last Crank Event Time (uint16, 1/1024 sec)
// ───────────────────────────────────────────────

void blePeripheralSendCps() {
    if (!cpsPowerChar) return;

    updateCrankCounter();

    uint8_t frame[8] = {0};
    size_t offset = 0;

    // Flags: bit5 = Crank Revolution Data present
    uint16_t flags = 0x0020;  // 0b0000_0000_0010_0000
    frame[offset++] = flags & 0xFF;
    frame[offset++] = (flags >> 8) & 0xFF;

    // Instantaneous Power (always present)
    int16_t power = (int16_t)g_trainerData.power;
    frame[offset++] = power & 0xFF;
    frame[offset++] = (power >> 8) & 0xFF;

    // Cumulative Crank Revolutions
    frame[offset++] = cumulativeCrankRevs & 0xFF;
    frame[offset++] = (cumulativeCrankRevs >> 8) & 0xFF;

    // Last Crank Event Time
    frame[offset++] = lastCrankEventTime & 0xFF;
    frame[offset++] = (lastCrankEventTime >> 8) & 0xFF;

    cpsPowerChar->setValue(frame, offset);
    cpsPowerChar->notify();
}

// ───────────────────────────────────────────────
// CSC Measurement notify (Telefon)
//
// Frame formátum:
// [0]    Flags (8 bit) — bit0: Wheel data, bit1: Crank data
// [1-4]  Cumulative Wheel Revolutions (uint32)
// [5-6]  Last Wheel Event Time (uint16, 1/1024 sec)
// [7-8]  Cumulative Crank Revolutions (uint16)
// [9-10] Last Crank Event Time (uint16, 1/1024 sec)
// ───────────────────────────────────────────────

void blePeripheralSendCsc() {
    if (!cscMeasureChar) return;

    updateWheelCounter();
    updateCrankCounter();

    uint8_t frame[11] = {0};
    size_t offset = 0;

    // Flags: bit0 = Wheel Revolution Data, bit1 = Crank Revolution Data
    uint8_t flags = 0x03;
    frame[offset++] = flags;

    // Cumulative Wheel Revolutions (uint32)
    frame[offset++] = cumulativeWheelRevs & 0xFF;
    frame[offset++] = (cumulativeWheelRevs >> 8) & 0xFF;
    frame[offset++] = (cumulativeWheelRevs >> 16) & 0xFF;
    frame[offset++] = (cumulativeWheelRevs >> 24) & 0xFF;

    // Last Wheel Event Time
    frame[offset++] = lastWheelEventTime & 0xFF;
    frame[offset++] = (lastWheelEventTime >> 8) & 0xFF;

    // Cumulative Crank Revolutions (uint16)
    frame[offset++] = cumulativeCrankRevs & 0xFF;
    frame[offset++] = (cumulativeCrankRevs >> 8) & 0xFF;

    // Last Crank Event Time
    frame[offset++] = lastCrankEventTime & 0xFF;
    frame[offset++] = (lastCrankEventTime >> 8) & 0xFF;

    cscMeasureChar->setValue(frame, offset);
    cscMeasureChar->notify();
}

// ───────────────────────────────────────────────
// Zwift → ESP32 → Suito vezérlés
// ───────────────────────────────────────────────

void handleControlFromClient(int targetPower) {
    sendResistanceCommandToSuito(targetPower);
}