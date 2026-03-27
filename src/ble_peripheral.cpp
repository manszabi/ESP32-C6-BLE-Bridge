#include "ble_peripheral.h"
#include "control_logic.h"
#include "cps_encoder.h"
#include "csc_encoder.h"
#include "data_model.h"
#include "config.h"

#include <NimBLEDevice.h>

// ───────────────────────────────────────────────
// BLE server + service/characteristic pointerek
// ───────────────────────────────────────────────

static NimBLEServer* bleServer = nullptr;

// FTMS (Zwift)
static NimBLECharacteristic* ftmsDataChar = nullptr;
static NimBLECharacteristic* ftmsControlPointChar = nullptr;

// CPS (Garmin)
static NimBLECharacteristic* cpsPowerChar = nullptr;

// CSC (Telefon)
static NimBLECharacteristic* cscMeasureChar = nullptr;

// ───────────────────────────────────────────────
// FreeRTOS Timer a counter-ek periodikus frissítéséhez
// ───────────────────────────────────────────────

static TimerHandle_t counterUpdateTimer = nullptr;
static const uint32_t COUNTER_UPDATE_PERIOD_MS = 150;

static void counterUpdateTimerCallback(TimerHandle_t xTimer) {
    (void)xTimer;
    cpsUpdateCrankCounter();
    cscUpdateWheelCounter();
}

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
// FTMS Control Point callback (Zwift → ESP32)
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
        }
    }
};

// ───────────────────────────────────────────────
// BLE Peripheral inicializálás
// ───────────────────────────────────────────────

bool blePeripheralInit() {
    bleServer = NimBLEDevice::createServer();

    // 1. FTMS Service (0x1826) — Zwift
    NimBLEService* ftmsService = bleServer->createService(NimBLEUUID((uint16_t)0x1826));

    NimBLECharacteristic* ftmsFeatureChar = ftmsService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2ACC),
        NIMBLE_PROPERTY::READ
    );
    uint8_t ftmsFeature[8] = {0};
    ftmsFeature[0] = 0x42;  // cadence + power
    ftmsFeature[5] = 0x40;  // Indoor Bike Simulation Parameters Supported
    ftmsFeatureChar->setValue(ftmsFeature, sizeof(ftmsFeature));

    ftmsDataChar = ftmsService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2AD2),
        NIMBLE_PROPERTY::NOTIFY
    );

    ftmsControlPointChar = ftmsService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2AD9),
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE
    );
    ftmsControlPointChar->setCallbacks(new FtmsControlPointCallback());

    // 2. CPS Service (0x1818) — Garmin
    NimBLEService* cpsService = bleServer->createService(NimBLEUUID((uint16_t)0x1818));

    NimBLECharacteristic* cpsFeatureChar = cpsService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A65), NIMBLE_PROPERTY::READ
    );
    uint8_t cpsFeature[4] = {0};
    cpsFeature[0] = 0x08;  // Crank Revolution Data Supported
    cpsFeatureChar->setValue(cpsFeature, sizeof(cpsFeature));

    NimBLECharacteristic* cpsSensorLocChar = cpsService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A5D), NIMBLE_PROPERTY::READ
    );
    uint8_t sensorLoc = 0x0D;  // Rear Hub
    cpsSensorLocChar->setValue(&sensorLoc, 1);

    cpsPowerChar = cpsService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A63), NIMBLE_PROPERTY::NOTIFY
    );

    // 3. CSC Service (0x1816) — Telefon
    NimBLEService* cscService = bleServer->createService(NimBLEUUID((uint16_t)0x1816));

    NimBLECharacteristic* cscFeatureChar = cscService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A5C), NIMBLE_PROPERTY::READ
    );
    uint8_t cscFeature[2] = {0};
    cscFeature[0] = 0x03;  // Wheel + Crank Revolution Data Supported
    cscFeatureChar->setValue(cscFeature, sizeof(cscFeature));

    cscMeasureChar = cscService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A5B), NIMBLE_PROPERTY::NOTIFY
    );

    // Szerver indítása
    bleServer->start();

    // FreeRTOS Timer a counter frissítéshez
    counterUpdateTimer = xTimerCreate(
        "CounterUpdateTimer",
        pdMS_TO_TICKS(COUNTER_UPDATE_PERIOD_MS),
        pdTRUE,
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

    adv->addServiceUUID(NimBLEUUID((uint16_t)0x1826));
    adv->addServiceUUID(NimBLEUUID((uint16_t)0x1818));
    adv->addServiceUUID(NimBLEUUID((uint16_t)0x1816));

    adv->start();
    Serial.println("BLE advertising started.");
}

// ───────────────────────────────────────────────
// FTMS Indoor Bike Data notify (Zwift)
// ───────────────────────────────────────────────

void blePeripheralSendFtms() {
    if (!ftmsDataChar) return;

    uint8_t frame[8] = {0};
    size_t offset = 0;

    // Flags: 0x0044 = bit2 (Cadence) + bit6 (Power), bit0=0 (Speed present)
    uint16_t flags = 0x0044;
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

    // Instantaneous Power (sint16, watts)
    int16_t rawPower = (int16_t)g_trainerData.power;
    frame[offset++] = rawPower & 0xFF;
    frame[offset++] = (rawPower >> 8) & 0xFF;

    ftmsDataChar->setValue(frame, offset);
    ftmsDataChar->notify();
}

// ───────────────────────────────────────────────
// CPS notify (Garmin)
// ───────────────────────────────────────────────

void blePeripheralSendCps() {
    if (!cpsPowerChar) return;

    uint8_t frame[8] = {0};
    size_t len = cpsEncode(frame, sizeof(frame));
    if (len > 0) {
        cpsPowerChar->setValue(frame, len);
        cpsPowerChar->notify();
    }
}

// ───────────────────────────────────────────────
// CSC notify (Telefon)
// ───────────────────────────────────────────────

void blePeripheralSendCsc() {
    if (!cscMeasureChar) return;

    uint8_t frame[11] = {0};
    size_t len = cscEncode(frame, sizeof(frame));
    if (len > 0) {
        cscMeasureChar->setValue(frame, len);
        cscMeasureChar->notify();
    }
}
