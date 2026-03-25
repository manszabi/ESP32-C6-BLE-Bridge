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

void schedulerInit() {
    lastFtmsNotify = millis();
    lastCpsNotify  = millis();
    lastCscNotify  = millis();
    lastSuitoCheck = millis();
}

void schedulerLoop() {
    uint32_t now = millis();

    // ───────────────────────────────────────────────
    // 1. Suito → ESP32 (kritikus)
    // ───────────────────────────────────────────────
    bleCentralLoop();

    // Stale data védelem (ha 500 ms óta nincs friss adat)
    if (now - g_trainerData.timestamp > 500) {
        g_trainerData.power   = 0;
        g_trainerData.cadence = 0;
        g_trainerData.speed   = 0.0f;
    }

    // ───────────────────────────────────────────────
    // 2. Zwift notify (FTMS) – magas prioritás
    // ───────────────────────────────────────────────
    if (now - lastFtmsNotify >= 50) {   // 20–50 ms ideális
        blePeripheralSendFtms();        // FTMS notify Zwiftnek
        lastFtmsNotify = now;
    }

    // ───────────────────────────────────────────────
    // 3. Garmin CPS notify – ritkított
    // ───────────────────────────────────────────────
    if (now - lastCpsNotify >= 300) {
        blePeripheralSendCps();         // Power Measurement
        lastCpsNotify = now;
    }

    // ───────────────────────────────────────────────
    // 4. Telefon CSC notify – ritkított
    // ───────────────────────────────────────────────
    if (now - lastCscNotify >= 300) {
        blePeripheralSendCsc();         // Speed/Cadence
        lastCscNotify = now;
    }

    // ───────────────────────────────────────────────
    // 5. Suito reconnect-logika
    // ───────────────────────────────────────────────
    if (now - lastSuitoCheck >= 2000) {   // 2 másodpercenként
        lastSuitoCheck = now;

        if (!bleCentralIsConnected()) {
            Serial.println("Suito disconnected! Reconnecting...");
            bleCentralConnectToSuito();
        }
    }
}
