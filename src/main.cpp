#include <Arduino.h>
#include "config.h"
#include "data_model.h"
#include "ble_central.h"
#include "ble_peripheral.h"

// ═══════════════════════════════════════════════
// Scheduler
// ═══════════════════════════════════════════════

static uint32_t lastFtmsNotify = 0;
static uint32_t lastCpsNotify = 0;
static uint32_t lastCscNotify = 0;
static uint32_t lastSuitoCheck = 0;

static uint32_t lastSuccessfulSuitoData = 0;
static uint32_t reconnectInterval = 1500;
static uint8_t  reconnectFailCount = 0;
static bool     isStale = false;

static void schedulerInit() {
    uint32_t now = millis();
    lastFtmsNotify = now;
    lastCpsNotify  = now;
    lastCscNotify  = now;
    lastSuitoCheck = now;
    lastSuccessfulSuitoData = now;

    reconnectInterval = 1500;
    reconnectFailCount = 0;
    isStale = false;

    Serial.println("Scheduler initialized - Adaptive reconnect + 1Hz Suito optimized + External Cadence priority");
}

static void schedulerLoop() {
    uint32_t now = millis();

    // 1. Derived mezők frissítése
    refreshDerivedFields();

    // 2. Stale védelem
    if (now - lastSuccessfulSuitoData > 1800) {
        if (!isStale) {
            g_trainerData.power = 0;
            g_trainerData.speed = 0.0f;
            isStale = true;
            reconnectFailCount++;
            reconnectInterval = 750;
        }
    } else {
        isStale = false;
    }

    // 3. Sikeres adat érkezésének detektálása
    if (g_trainerData.timestamp > lastSuccessfulSuitoData) {
        lastSuccessfulSuitoData = g_trainerData.timestamp;
        reconnectFailCount = 0;
        reconnectInterval = 1500;
    }

    // 4. Adaptív reconnect logika
    if (now - lastSuitoCheck >= reconnectInterval) {
        lastSuitoCheck = now;

        ScanResults sr = bleScanGetResults();
        if (!sr.ftmsFound && !bleScanIsRunning()) {
            bleScanStart();
        }

        if (!bleScanIsRunning()) {
            // Suito reconnect
            if (!bleCentralIsConnected()) {
                if (bleCentralConnectToSuito()) {
                    reconnectFailCount = 0;
                    reconnectInterval = 1500;
                    Serial.println("Suito connected successfully");
                } else if (sr.ftmsFound) {
                    reconnectFailCount++;
                    if (reconnectFailCount > 5) {
                        reconnectInterval = 3500;
                    } else if (reconnectFailCount > 2) {
                        reconnectInterval = 2000;
                    } else {
                        reconnectInterval = 750;
                    }
                    if (reconnectFailCount % 3 == 0) {
                        Serial.printf("Suito reconnect attempt #%d, interval = %d ms\n",
                                    reconnectFailCount, reconnectInterval);
                    }
                }
            } else {
                if (reconnectFailCount > 0) {
                    reconnectFailCount = 0;
                    reconnectInterval = 1500;
                }
            }

            // HRM és Cadence connect
            if (!bleCentralHrmIsConnected())      bleCentralHrmConnect();
            if (!bleCentralCadenceIsConnected())  bleCentralCadenceConnect();

            // Ha FTMS megvan, de HRM/CAD még nem, időnként scanneljünk
            if (sr.ftmsFound && (!sr.hrmFound || !sr.cadenceFound)) {
                static uint32_t lastOptionalScan = 0;
                if (now - lastOptionalScan > 30000) {
                    lastOptionalScan = now;
                    bleScanStart();
                }
            }
        }
    }

    // 5. FTMS notify Zwift felé (200 ms)
    if (now - lastFtmsNotify >= 200) {
        if (isFtmsClientConnected()) {
            blePeripheralSendFtms();
        }
        lastFtmsNotify = now;
    }

    // 6. CPS notify (Garmin)
    if (now - lastCpsNotify >= 1000) {
        if (isCpsClientConnected()) {
            blePeripheralSendCps();
        }
        lastCpsNotify = now;
    }

    // 7. CSC notify (telefon/app)
    if (now - lastCscNotify >= 1500) {
        if (isCscClientConnected()) {
            blePeripheralSendCsc();
        }
        lastCscNotify = now;
    }
}

// ═══════════════════════════════════════════════
// Arduino setup / loop
// ═══════════════════════════════════════════════

void setup() {
#if DEBUG_SERIAL
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32C6 Suito BLE Bridge starting...");
#endif

    // Central oldali init (NimBLE + scan setup)
    bleCentralInit();

    // Peripheral oldali init
    blePeripheralInit();
    blePeripheralStartAdvertising();

    // Első scan indítása
    bleScanStart();

    // Scheduler
    schedulerInit();
}

void loop() {
    schedulerLoop();
}
