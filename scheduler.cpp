#include "scheduler.h"
#include "data_model.h"
#include "ble_central.h"
#include "ble_central_hrm.h"
#include "ble_central_cadence.h"
#include "ble_peripheral.h"
#include "config.h"
#include <Arduino.h>

// Globális változók (scheduler.h-ban is deklaráld őket)
uint32_t lastFtmsNotify = 0;
uint32_t lastCpsNotify = 0;
uint32_t lastCscNotify = 0;
uint32_t lastSuitoCheck = 0;

// Adaptív reconnect + Suito 1Hz optimalizált változók
static uint32_t lastSuccessfulSuitoData = 0;
static uint32_t reconnectInterval = 1500;     // stabil állapotban 1.5 másodperc
static uint8_t  reconnectFailCount = 0;
static bool     isStale = false;

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

    Serial.println("Scheduler initialized - Adaptive reconnect + 1Hz Suito + External Cadence priority");
}

void schedulerLoop() {
    uint32_t now = millis();

    // ===================================================================
    // 1. Legmagasabb prioritás: Minden central adat fogadása
    // ===================================================================
    bleCentralLoop();           // Suito FTMS (power, speed, cadence stb.)
    bleCentralHrmLoop();        // HRM szenzor
    bleCentralCadenceLoop();    // Külső cadence szenzor (ez a legfontosabb neked)

    // ===================================================================
    // 2. Derived mezők frissítése – KÖZVETLENÜL az adatfogadás után!
    //    Itt döntjük el a cadence prioritást (külső szenzor > Suito)
    // ===================================================================
    refreshDerivedFields();     // ← Áthelyezve ide! (cadence választás + smoothing)

    // ===================================================================
    // 3. Stale védelem (1 Hz-es Suito-hoz lazábban)
    // ===================================================================
    if (now - lastSuccessfulSuitoData > 1800) {   // 1.8 másodperc után
        if (!isStale) {
            g_trainerData.power = 0;
            g_trainerData.speed = 0.0f;
            isStale = true;
            reconnectFailCount++;
            reconnectInterval = 750;               // gyors üzemmód
        }
    } else {
        isStale = false;
    }

    // ===================================================================
    // 4. Új adat érkezett a Suitótól? → reseteljük a hibaszámlálót
    // ===================================================================
    if (g_trainerData.timestamp > lastSuccessfulSuitoData) {
        lastSuccessfulSuitoData = g_trainerData.timestamp;
        reconnectFailCount = 0;
        reconnectInterval = 1500;                  // vissza stabil állapotba
    }

    // ===================================================================
    // 5. Adaptív reconnect check
    // ===================================================================
    if (now - lastSuitoCheck >= reconnectInterval) {
        lastSuitoCheck = now;

        bool currentlyConnected = bleCentralIsConnected();

        if (!currentlyConnected) {
            bleCentralConnectToSuito();
            reconnectFailCount++;

            // Exponential backoff
            if (reconnectFailCount > 5) {
                reconnectInterval = 3500;
            } else if (reconnectFailCount > 2) {
                reconnectInterval = 2000;
            } else {
                reconnectInterval = 750;           // gyors próbálkozás
            }

            if (reconnectFailCount % 3 == 0 && reconnectFailCount > 0) {
                Serial.printf("Suito reconnect attempt #%d, interval=%d ms\n", 
                              reconnectFailCount, reconnectInterval);
            }
        } else {
            // Sikeres kapcsolat
            if (reconnectFailCount > 0) {
                reconnectFailCount = 0;
                reconnectInterval = 1500;
                Serial.println("Suito reconnected successfully");
            }
        }

        // HRM és külső cadence reconnect (egyszerű)
        if (!bleCentralHrmIsConnected())      bleCentralHrmConnect();
        if (!bleCentralCadenceIsConnected())  bleCentralCadenceConnect();
    }

    // ===================================================================
    // 6. FTMS notify Zwift felé (200 ms – optimális 1 Hz-es Suito esetén)
    // ===================================================================
    if (now - lastFtmsNotify >= 200) {
        if (isFtmsClientConnected() && !isStale) {
            blePeripheralSendFtms();        // itt már a helyes (külső) cadence megy
        }
        lastFtmsNotify = now;
    }

    // ===================================================================
    // 7. CPS notify (Garmin)
    // ===================================================================
    if (now - lastCpsNotify >= 1000) {
        if (isCpsClientConnected()) {
            blePeripheralSendCps();
        }
        lastCpsNotify = now;
    }

    // ===================================================================
    // 8. CSC notify (telefon) – külön csatorna a külső cadence-hez
    // ===================================================================
    if (now - lastCscNotify >= 1500) {
        if (isCscClientConnected()) {
            blePeripheralSendCsc();         // itt is a preferált cadence megy
        }
        lastCscNotify = now;
    }
}
