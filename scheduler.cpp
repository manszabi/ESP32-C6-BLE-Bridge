#include "scheduler.h"
#include "data_model.h"
#include "ble_central.h"
#include "ble_central_hrm.h"
#include "ble_central_cadence.h"
#include "ble_peripheral.h"
#include "config.h"

uint32_t lastFtmsNotify = 0;
uint32_t lastCpsNotify  = 0;
uint32_t lastCscNotify  = 0;
uint32_t lastSuitoCheck = 0;

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

    // 1. Central oldali loop-ok (nem blokkolóak!)
    bleCentralLoop();          // Suito FTMS
    bleCentralHrmLoop();       // HRM
    bleCentralCadenceLoop();   // Cadence szenzor

    // 2. Stale védelem – Suito adat
    if (now - g_trainerData.timestamp > 500) {
        if (!isStale) {
            g_trainerData.power   = 0;
            g_trainerData.speed   = 0.0f;
            // cadence-t a refreshDerivedFields úgyis kezeli
            isStale = true;
        }
    } else {
        isStale = false;
    }

    // 3. Derived mezők frissítése (cadence forrás választás)
    refreshDerivedFields();

    // 4. FTMS → Zwift (50 ms)
    if (now - lastFtmsNotify >= 50) {
        if (isFtmsClientConnected()) {
            blePeripheralSendFtms();
        }
        lastFtmsNotify = now;
    }

    // 5. CPS → Garmin (1 Hz)
    if (now - lastCpsNotify >= 1000) {
        if (isCpsClientConnected()) {
            blePeripheralSendCps();
        }
        lastCpsNotify = now;
    }

    // 6. CSC → Telefon (1 Hz)
    if (now - lastCscNotify >= 1000) {
        if (isCscClientConnected()) {
            blePeripheralSendCsc();
        }
        lastCscNotify = now;
    }

    // 7. Reconnect – Suito / HRM / Cadence (2 s)
    if (now - lastSuitoCheck >= 2000) {
        lastSuitoCheck = now;

        if (!bleCentralIsConnected()) {
            Serial.println("Suito disconnected! Reconnecting...");
            bleCentralConnectToSuito();
        }

        if (!bleCentralHrmIsConnected()) {
            Serial.println("HRM disconnected! Reconnecting...");
            bleCentralHrmConnect();
        }

        if (!bleCentralCadenceIsConnected()) {
            Serial.println("Cadence sensor disconnected! Reconnecting...");
            bleCentralCadenceConnect();
        }
    }
}
