#include "ble_peripheral.h"
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

// CPS (Garmin)
static NimBLEService* cpsService = nullptr;
static NimBLECharacteristic* cpsPowerChar = nullptr;

// CSC (Telefon)
static NimBLEService* cscService = nullptr;
static NimBLECharacteristic* cscMeasureChar = nullptr;


// ───────────────────────────────────────────────
// Kliens-ellenőrzés
// ───────────────────────────────────────────────

bool isFtmsClientConnected() {
    return ftmsDataChar && ftmsDataChar->getSubscribedCount() > 0;
}

bool isCpsClientConnected() {
    return cpsPowerChar && cpsPowerChar->getSubscribedCount() > 0;
}

bool isCscClientConnected() {
    return cscMeasureChar && cscMeasureChar->getSubscribedCount() > 0;
}


// ───────────────────────────────────────────────
// Zwift → ESP32 → Suito vezérlés (Control Point)
// ───────────────────────────────────────────────

class FtmsControlPointCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        std::string val = c->getValue();

        // TODO: FTMS Control Point parancs dekódolása
        // Példa: target power kinyerése
        int targetPower = 0;

        // Meghívjuk a központi vezérlő logikát
        handleControlFromClient(targetPower);
    }
};


// ───────────────────────────────────────────────
// BLE Peripheral inicializálás
// ───────────────────────────────────────────────

bool blePeripheralInit() {
    NimBLEDevice::init(BRIDGE_DEVICE_NAME);
    bleServer = NimBLEDevice::createServer();

    // ───────────────────────────────────────────
    // 1. FTMS Service (Zwift)
    // ───────────────────────────────────────────
    ftmsService = bleServer->createService("1826");

    ftmsDataChar = ftmsService->createCharacteristic(
        "2AD2",  // Indoor Bike Data
        NIMBLE_PROPERTY::NOTIFY
    );

    ftmsControlPointChar = ftmsService->createCharacteristic(
        "2AD9",  // Fitness Machine Control Point
        NIMBLE_PROPERTY::WRITE
    );
    ftmsControlPointChar->setCallbacks(new FtmsControlPointCallback());

    ftmsService->start();


    // ───────────────────────────────────────────
    // 2. CPS Service (Garmin)
    // ───────────────────────────────────────────
    cpsService = bleServer->createService("1818");

    cpsPowerChar = cpsService->createCharacteristic(
        "2A63",  // Cycling Power Measurement
        NIMBLE_PROPERTY::NOTIFY
    );

    cpsService->start();


    // ───────────────────────────────────────────
    // 3. CSC Service (Telefon)
    // ───────────────────────────────────────────
    cscService = bleServer->createService("1816");

    cscMeasureChar = cscService->createCharacteristic(
        "2A5B",  // CSC Measurement
        NIMBLE_PROPERTY::NOTIFY
    );

    cscService->start();

    return true;
}


// ───────────────────────────────────────────────
// Advertising indítása
// ───────────────────────────────────────────────

void blePeripheralStartAdvertising() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();

    adv->addServiceUUID("1826"); // FTMS
    adv->addServiceUUID("1818"); // CPS
    adv->addServiceUUID("1816"); // CSC

    adv->setScanResponse(true);
    adv->start();

    Serial.println("BLE advertising started.");
}


// ───────────────────────────────────────────────
// FTMS notify (Zwift)
// ───────────────────────────────────────────────

void blePeripheralSendFtms() {
    if (!ftmsDataChar) return;

    // FTMS Indoor Bike Data frame összeállítása
    // Ez csak váz, a valós FTMS frame bonyolultabb
    uint8_t frame[8] = {0};

    int power   = g_trainerData.power;
    int cadence = g_trainerData.cadence;
    float speed = g_trainerData.speed;

    // Példa: egyszerűsített FTMS frame
    frame[0] = power & 0xFF;
    frame[1] = (power >> 8) & 0xFF;
    frame[2] = cadence;
    frame[3] = (uint8_t)(speed * 10);

    ftmsDataChar->setValue(frame, sizeof(frame));
    ftmsDataChar->notify();
}


// ───────────────────────────────────────────────
// CPS notify (Garmin)
// ───────────────────────────────────────────────

void blePeripheralSendCps() {
    if (!cpsPowerChar) return;

    uint8_t frame[4] = {0};

    int power = g_trainerData.power;

    frame[0] = power & 0xFF;
    frame[1] = (power >> 8) & 0xFF;

    cpsPowerChar->setValue(frame, sizeof(frame));
    cpsPowerChar->notify();
}


// ───────────────────────────────────────────────
// CSC notify (Telefon)
// ───────────────────────────────────────────────

void blePeripheralSendCsc() {
    if (!cscMeasureChar) return;

    uint8_t frame[4] = {0};

    int cadence = g_trainerData.cadence;

    frame[0] = 0x02; // csak crank data flag
    frame[1] = cadence & 0xFF;
    frame[2] = (cadence >> 8) & 0xFF;

    cscMeasureChar->setValue(frame, sizeof(frame));
    cscMeasureChar->notify();
}


// ───────────────────────────────────────────────
// Zwift → ESP32 → Suito vezérlés
// ───────────────────────────────────────────────

void handleControlFromClient(int targetPower) {
    // Ezt a Suito FTMS Control Point felé továbbítjuk
    sendResistanceCommandToSuito(targetPower);
}
