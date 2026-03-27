#include "scheduler.h"
#include "data_model.h"
#include "ble_central.h"
#include "ble_peripheral.h"
#include "config.h"

// ════════════════════════════════════════════════
// Időzítők
// ════════════════════════════════════════════════

static uint32_t lastFtmsNotify = 0;
static uint32_t lastCpsNotify = 0;
static uint32_t lastCscNotify = 0;
static uint32_t lastSuitoCheck = 0;

// Adaptív reconnect
static uint32_t lastSuccessfulSuitoData = 0;
static uint32_t reconnectInterval = 1500;
static uint8_t  reconnectFailCount = 0;
static bool     isStale = false;

// ════════════════════════════════════════════════
// Inicializálás
// ════════════════════════════════════════════════

void schedulerInit() {
    uint32_t now = millis();
    lastFtmsNotify = now;
    lastCpsNotify  = now;
    lastCscNotify  = now;
    lastSuitoCheck = now;
    lastSuccessfulSuitoData = now;

    reconnectInterval = 1500;
    reconnectFailCount = 0;
    isStale = false;

    Serial.println("Scheduler initialized");
}

// ════════════════════════════════════════════════
// Fő scheduler ciklus
// ════════════════════════════════════════════════

void schedulerLoop() {
    uint32_t now = millis();

    // 1. Stale védelem — 1.8 s timeout
    if (now - lastSuccessfulSuitoData > 1800) {
        if (!isStale) {
            g_trainerData.power = 0;
            g_trainerData.cadence = 0;
            g_trainerData.speed = 0.0f;
            isStale = true;
            reconnectFailCount++;
            reconnectInterval = 750;
        }
    } else {
        isStale = false;
    }

    // 2. Sikeres adat érkezésének detektálása
    if (g_trainerData.timestamp > lastSuccessfulSuitoData) {
        lastSuccessfulSuitoData = g_trainerData.timestamp;
        reconnectFailCount = 0;
        reconnectInterval = 1500;
    }

    // 3. Adaptív reconnect logika
    if (now - lastSuitoCheck >= reconnectInterval) {
        lastSuitoCheck = now;

        if (!bleScanFtmsFound() && !bleScanIsRunning()) {
            bleScanStart();
        }

        if (!bleScanIsRunning()) {
            if (!bleCentralIsConnected()) {
                if (bleCentralConnectToSuito()) {
                    reconnectFailCount = 0;
                    reconnectInterval = 1500;
                    Serial.println("Suito connected successfully");
                } else if (bleScanFtmsFound()) {
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
        }
    }

    // 4. FTMS notify Zwift felé (200 ms)
    if (now - lastFtmsNotify >= 200) {
        if (isFtmsClientConnected()) {
            blePeripheralSendFtms();
        }
        lastFtmsNotify = now;
    }

    // 5. CPS notify — Garmin (1000 ms)
    if (now - lastCpsNotify >= 1000) {
        if (isCpsClientConnected()) {
            blePeripheralSendCps();
        }
        lastCpsNotify = now;
    }

    // 6. CSC notify — Telefon/app (1000 ms)
    if (now - lastCscNotify >= 1000) {
        if (isCscClientConnected()) {
            blePeripheralSendCsc();
        }
        lastCscNotify = now;
    }
}
