#include "ble_peripheral.h"
#include "ble_central.h"
#include "data_model.h"
#include "config.h"

#include <NimBLEDevice.h>
#include <freertos/timers.h>        // FreeRTOS timerhez

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
// Kumulatív számlálók (CPS és CSC-hez)
// ───────────────────────────────────────────────

static uint16_t cumulativeCrankRevs = 0;
static uint16_t lastCrankEventTime  = 0;   // 1/1024 sec egységben

static uint32_t cumulativeWheelRevs = 0;
static uint16_t lastWheelEventTime  = 0;   // 1/1024 sec egységben

static uint32_t lastCrankUpdateMs = 0;
static uint32_t lastWheelUpdateMs = 0;

// ───────────────────────────────────────────────
// FreeRTOS Timer a counter-ek periodikus frissítéséhez
// ───────────────────────────────────────────────
static TimerHandle_t counterUpdateTimer = nullptr;
static const uint32_t COUNTER_UPDATE_PERIOD_MS = 150;   // 100-200 ms ajánlott tartomány

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

        if (opcode == 0x05 && val.size() >= 3) {
            int16_t targetPower = (int16_t)(d[1] | (d[2] << 8));
            handleControlFromClient(targetPower);

            // TODO: Response Code indication küldése (0x80 opcode)
        }
        // TODO: 0x11 Indoor Bike Simulation Parameters kezelése
    }
};

// ───────────────────────────────────────────────
// Timer callback – periodikus counter frissítés
// ───────────────────────────────────────────────
static void counterUpdateTimerCallback(TimerHandle_t xTimer) {
    (void)xTimer;  // nem használjuk

    updateCrankCounter();
    updateWheelCounter();
}

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
    ftmsFeatureChar = ftmsService->createCharacteristic(
        "2ACC",
        NIMBLE_PROPERTY::READ
    );

    // Bit 1: Cadence Supported, Bit 6: Power Measurement Supported
    // Bit 2 (Total Distance) szándékosan kikapcsolva, mert nem küldjük
    uint8_t ftmsFeature[8] = {0};
    ftmsFeature[0] = 0x42;  // 0b01000010 → cadence + power
    ftmsFeature[5] = 0x40;  // Bit 14: Indoor Bike Simulation Parameters Supported
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

    cpsFeatureChar = cpsService->createCharacteristic("2A65", NIMBLE_PROPERTY::READ);
    uint8_t cpsFeature[4] = {0};
    cpsFeature[0] = 0x08;  // Bit 3: Crank Revolution Data Supported
    cpsFeatureChar->setValue(cpsFeature, sizeof(cpsFeature));

    cpsSensorLocChar = cpsService->createCharacteristic("2A5D", NIMBLE_PROPERTY::READ);
    uint8_t sensorLoc = 0x0D;  // Rear Hub
    cpsSensorLocChar->setValue(&sensorLoc, 1);

    cpsPowerChar = cpsService->createCharacteristic("2A63", NIMBLE_PROPERTY::NOTIFY);

    // ─────────────────────────────────────────
    // 3. CSC Service (0x1816) — Telefon
    // ─────────────────────────────────────────
    cscService = bleServer->createService("1816");

    cscFeatureChar = cscService->createCharacteristic("2A5C", NIMBLE_PROPERTY::READ);
    uint8_t cscFeature[2] = {0};
    cscFeature[0] = 0x03;  // Wheel + Crank Revolution Data Supported
    cscFeatureChar->setValue(cscFeature, sizeof(cscFeature));

    cscMeasureChar = cscService->createCharacteristic("2A5B", NIMBLE_PROPERTY::NOTIFY);

    // ─────────────────────────────────────────
    // FreeRTOS Timer a counter-ek periodikus frissítéséhez
    // ─────────────────────────────────────────
    counterUpdateTimer = xTimerCreate(
        "CounterUpdateTimer",
        pdMS_TO_TICKS(COUNTER_UPDATE_PERIOD_MS),
        pdTRUE,                                   // auto-reload
        nullptr,
        counterUpdateTimerCallback
    );

    if (counterUpdateTimer != nullptr) {
        if (xTimerStart(counterUpdateTimer, 0) == pdPASS) {
            Serial.printf("Counter update timer started (every %d ms)\n", COUNTER_UPDATE_PERIOD_MS);
        } else {
            Serial.println("WARNING: Failed to start counter update timer!");
        }
    } else {
        Serial.println("ERROR: Failed to create counter update timer!");
    }

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
// Crank counter frissítése (CPS + CSC-hez)
// ───────────────────────────────────────────────
static void updateCrankCounter() {
    uint32_t now = millis();
    int16_t cadence = g_trainerData.cadence;

    if (cadence <= 0 || now <= lastCrankUpdateMs) {
        lastCrankUpdateMs = now;
        return;
    }

    uint32_t elapsedMs = now - lastCrankUpdateMs;
    float revs = (cadence / 60.0f) * (elapsedMs / 1000.0f);

    if (revs > 0.005f) {
        cumulativeCrankRevs = (cumulativeCrankRevs + (uint16_t)(revs + 0.5f)) & 0xFFFF;

        uint32_t ticks = (elapsedMs * 1024UL) / 1000UL;
        lastCrankEventTime = (lastCrankEventTime + (uint16_t)ticks) & 0xFFFF;

        lastCrankUpdateMs = now;
    } else {
        lastCrankUpdateMs = now;
    }
}

// ───────────────────────────────────────────────
// Wheel counter frissítése (CSC-hez)
// ───────────────────────────────────────────────
static void updateWheelCounter() {
    uint32_t now = millis();
    float speedKmh = g_trainerData.speed;

    if (speedKmh < 0.1f || now <= lastWheelUpdateMs) {
        lastWheelUpdateMs = now;
        return;
    }

    uint32_t elapsedMs = now - lastWheelUpdateMs;
    float distMeters = (speedKmh / 3.6f) * (elapsedMs / 1000.0f);
    float revs = distMeters / WHEEL_CIRCUMFERENCE_M;

    if (revs > 0.005f) {
        cumulativeWheelRevs += (uint32_t)(revs + 0.5f);

        uint32_t ticks = (elapsedMs * 1024UL) / 1000UL;
        lastWheelEventTime = (lastWheelEventTime + (uint16_t)ticks) & 0xFFFF;

        lastWheelUpdateMs = now;
    } else {
        lastWheelUpdateMs = now;
    }
}

// ───────────────────────────────────────────────
// FTMS Indoor Bike Data notify (Zwift)
// ───────────────────────────────────────────────
void blePeripheralSendFtms() {
    if (!ftmsDataChar) return;

    uint8_t frame[8] = {0};
    size_t offset = 0;

    // Flags: bit1 (Speed) + bit2 (Cadence) + bit6 (Power)
    uint16_t flags = 0x0046;

    frame[offset++] = flags & 0xFF;
    frame[offset++] = (flags >> 8) & 0xFF;

    // Instantaneous Speed (0.01 km/h)
    uint16_t rawSpeed = (uint16_t)(g_trainerData.speed * 100.0f + 0.5f);
    frame[offset++] = rawSpeed & 0xFF;
    frame[offset++] = (rawSpeed >> 8) & 0xFF;

    // Instantaneous Cadence (0.5 rpm)
    uint16_t rawCadence = (uint16_t)(g_trainerData.cadence * 2.0f + 0.5f);
    frame[offset++] = rawCadence & 0xFF;
    frame[offset++] = (rawCadence >> 8) & 0xFF;

    // Instantaneous Power (sint16)
    int16_t rawPower = (int16_t)g_trainerData.power;
    frame[offset++] = rawPower & 0xFF;
    frame[offset++] = (rawPower >> 8) & 0xFF;

    ftmsDataChar->setValue(frame, offset);
    ftmsDataChar->notify();
}

// ───────────────────────────────────────────────
// CPS Cycling Power Measurement notify (Garmin)
// ───────────────────────────────────────────────
void blePeripheralSendCps() {
    if (!cpsPowerChar) return;

    updateCrankCounter();        // biztosíték

    uint8_t frame[8] = {0};
    size_t offset = 0;

    uint16_t flags = 0x0020;     // Crank Revolution Data present

    frame[offset++] = flags & 0xFF;
    frame[offset++] = (flags >> 8) & 0xFF;

    int16_t power = (int16_t)g_trainerData.power;
    frame[offset++] = power & 0xFF;
    frame[offset++] = (power >> 8) & 0xFF;

    frame[offset++] = cumulativeCrankRevs & 0xFF;
    frame[offset++] = (cumulativeCrankRevs >> 8) & 0xFF;

    frame[offset++] = lastCrankEventTime & 0xFF;
    frame[offset++] = (lastCrankEventTime >> 8) & 0xFF;

    cpsPowerChar->setValue(frame, offset);
    cpsPowerChar->notify();
}

// ───────────────────────────────────────────────
// CSC Measurement notify (Telefon)
// ───────────────────────────────────────────────
void blePeripheralSendCsc() {
    if (!cscMeasureChar) return;

    updateWheelCounter();
    updateCrankCounter();

    uint8_t frame[11] = {0};
    size_t offset = 0;

    uint8_t flags = 0x03;        // Wheel + Crank data present

    frame[offset++] = flags;

    // Cumulative Wheel Revolutions (uint32)
    frame[offset++] = cumulativeWheelRevs & 0xFF;
    frame[offset++] = (cumulativeWheelRevs >> 8) & 0xFF;
    frame[offset++] = (cumulativeWheelRevs >> 16) & 0xFF;
    frame[offset++] = (cumulativeWheelRevs >> 24) & 0xFF;

    // Last Wheel Event Time
    frame[offset++] = lastWheelEventTime & 0xFF;
    frame[offset++] = (lastWheelEventTime >> 8) & 0xFF;

    // Cumulative Crank Revolutions
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