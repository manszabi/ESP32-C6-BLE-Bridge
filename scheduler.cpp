#include "scheduler.h"
#include "data_model.h"
#include "ble_central.h"
#include "ble_peripheral.h"
#include "config.h"

// Időbélyegek
uint32_t lastFtmsNotify = 0;
uint32_t lastCpsNotify  = 0;
uint32_t lastCscNotify  = 0;
uint32_t lastSuitoCheck = 0;

// Stale flag
static bool isStale = false;

void schedulerInit() {
    uint32_t now = millis();
    lastFtmsNotify = now;
    lastCpsNotify  = now;
    lastCscNotify  = now;
    lastSuitoCheck = now;
    isStale        = false;
}

void schedulerLoop() {
    uint32_t now = millis();

    // ───────────────────────────────────────────────
    // 1. Suito → ESP32 (kritikus)
    // ───────────────────────────────────────────────
    bleCentralLoop();

    // Stale data védelem
    if (now - g_trainerData.timestamp > 500) {
        if (!isStale) {
            g_trainerData.power   = 0;
            g_trainerData.cadence = 0;
            g_trainerData.speed   = 0.0f;
            isStale = true;
        }
    } else {
        isStale = false;
    }

    // ───────────────────────────────────────────────
    // 2. Zwift FTMS notify – magas prioritás
    // ───────────────────────────────────────────────
    if (now - lastFtmsNotify >= 50) {
        if (isFtmsClientConnected()) {
            blePeripheralSendFtms();
        }
        lastFtmsNotify = now;
    }

    // ───────────────────────────────────────────────
    // 3. Garmin CPS notify – 1 Hz
    // ───────────────────────────────────────────────
    if (now - lastCpsNotify >= 1000) {
        if (isCpsClientConnected()) {
            blePeripheralSendCps();
        }
        lastCpsNotify = now;
    }

    // ───────────────────────────────────────────────
    // 4. Telefon CSC notify – 1 Hz
    // ───────────────────────────────────────────────
    if (now - lastCscNotify >= 1000) {
        if (isCscClientConnected()) {
            blePeripheralSendCsc();
        }
        lastCscNotify = now;
    }

    // ───────────────────────────────────────────────
    // 5. Suito reconnect – 2 másodpercenként
    // ───────────────────────────────────────────────
    if (now - lastSuitoCheck >= 2000) {
        lastSuitoCheck = now;

        if (!bleCentralIsConnected()) {
            Serial.println("Suito disconnected! Reconnecting...");
            bleCentralConnectToSuito();
        }
    }
}
