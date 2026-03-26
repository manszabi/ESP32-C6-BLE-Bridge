#include "scheduler.h"
#include "data_model.h"
#include "ble_central.h"
#include "ble_central_hrm.h"
#include "ble_central_cadence.h"
#include "ble_peripheral.h"
#include "config.h"
#include <Arduino.h>

// ================================================
// Globális változók
// ================================================
uint32_t lastFtmsNotify = 0;
uint32_t lastCpsNotify = 0;
uint32_t lastCscNotify = 0;
uint32_t lastSuitoCheck = 0;

// Adaptív reconnect + 1 Hz-es Suito optimalizált változók
static uint32_t lastSuccessfulSuitoData = 0;   // utolsó időpont, amikor új adatot kaptunk a Suitótól
static uint32_t reconnectInterval = 1500;      // aktuális reconnect ellenőrzési intervallum (ms)
static uint8_t  reconnectFailCount = 0;        // hányszor sikertelen volt a reconnect egymás után
static bool     isStale = false;               // jelzi, ha a Suito adatai elavultak

// ================================================
// Inicializálás
// ================================================
void schedulerInit() {
    uint32_t now = millis();

    // Minden időzítőt a jelenlegi időpontra állítunk
    lastFtmsNotify = now;
    lastCpsNotify  = now;
    lastCscNotify  = now;
    lastSuitoCheck = now;
    lastSuccessfulSuitoData = now;

    // Adaptív reconnect kezdeti állapot
    reconnectInterval = 1500;      // stabil állapotban 1.5 másodpercenként ellenőrzünk
    reconnectFailCount = 0;
    isStale = false;

    Serial.println("Scheduler v2 initialized - Adaptive reconnect + 1Hz Suito optimized + External Cadence priority");
}

// ================================================
// Fő scheduler ciklus - minden loop()-ban meghívódik
// ================================================
void schedulerLoop() {
    uint32_t now = millis();

    // ===================================================================
    // 1. Legmagasabb prioritás: Central oldal adatfogadása
    //    Itt érkeznek be a nyers adatok az összes BLE eszközről
    // ===================================================================
    bleCentralLoop();           // Elite Suito FTMS (power, speed, cadence, resistance stb.)
    bleCentralHrmLoop();        // Külső szívritmus szenzor
    bleCentralCadenceLoop();    // Külső cadence szenzor ← ez kritikus a te setupodban

    // ===================================================================
    // 2. Derived mezők frissítése (nagyon fontos!)
    //    Közvetlenül az adatfogadás után fut, hogy a legfrissebb adatok alapján döntsünk
    //    Itt történik a cadence prioritás: Külső szenzor > Suito cadence
    // ===================================================================
    refreshDerivedFields();     // cadence választás + esetleges smoothing

    // ===================================================================
    // 3. Stale védelem
    //    Ha túl régóta nem érkezett új adat a Suitótól, nullázzuk a kritikus értékeket,
    //    hogy a Zwift ne kapjon elavult teljesítményadatot
    // ===================================================================
    if (now - lastSuccessfulSuitoData > 1800) {        // 1.8 másodperc (1 Hz-es Suito-hoz illeszkedik)
        if (!isStale) {
            g_trainerData.power = 0;
            g_trainerData.speed = 0.0f;
            isStale = true;
            reconnectFailCount++;                      // növeljük a hiba számlálót
            reconnectInterval = 750;                   // gyors üzemmódba váltunk
        }
    } else {
        isStale = false;
    }

    // ===================================================================
    // 4. Sikeres adat érkezésének detektálása
    //    Ha új adat jött a Suitótól, visszaállítjuk a stabil állapotot
    // ===================================================================
    if (g_trainerData.timestamp > lastSuccessfulSuitoData) {
        lastSuccessfulSuitoData = g_trainerData.timestamp;
        reconnectFailCount = 0;                        // hiba számláló nullázása
        reconnectInterval = 1500;                      // vissza stabil intervallumra
    }

    // ===================================================================
    // 5. Adaptív reconnect logika
    //    Stabil esetben ritkábban, probléma esetén gyorsabban próbálkozik
    // ===================================================================
    if (now - lastSuitoCheck >= reconnectInterval) {
        lastSuitoCheck = now;

        bool currentlyConnected = bleCentralIsConnected();

        if (!currentlyConnected) {
            // Reconnect próbálkozás a Suitóhoz
            bleCentralConnectToSuito();
            reconnectFailCount++;

            // Exponential backoff: minél többször hibázik, annál ritkábban próbálkozik
            if (reconnectFailCount > 5) {
                reconnectInterval = 3500;      // maximum 3.5 másodperc
            } else if (reconnectFailCount > 2) {
                reconnectInterval = 2000;
            } else {
                reconnectInterval = 750;       // gyors próbálkozás
            }

            // Debug információ (nem túl gyakran)
            if (reconnectFailCount % 3 == 0 && reconnectFailCount > 0) {
                Serial.printf("Suito reconnect attempt #%d, interval = %d ms\n", 
                              reconnectFailCount, reconnectInterval);
            }
        } 
        else {
            // Kapcsolat rendben van
            if (reconnectFailCount > 0) {
                reconnectFailCount = 0;
                reconnectInterval = 1500;
                Serial.println("Suito reconnected successfully");
            }
        }

        // Egyéb central eszközök reconnect-je (egyszerűbb logika)
        if (!bleCentralHrmIsConnected())      bleCentralHrmConnect();
        if (!bleCentralCadenceIsConnected())  bleCentralCadenceConnect();
    }

    // ===================================================================
    // 6. FTMS notify Zwift felé (200 ms intervallum)
    //    1 Hz-es Suito esetén ez az optimális érték: elég sűrű, de nem spam
    // ===================================================================
    
    /**
    if (now - lastFtmsNotify >= 200) {
        if (isFtmsClientConnected() && !isStale) {
            blePeripheralSendFtms();        // már a helyesen választott cadence-dzsel
        }
        lastFtmsNotify = now;
    }
    */
    
    if (now - lastFtmsNotify >= 200) {
        if (isFtmsClientConnected()) {        // ← stale esetén IS küld
            blePeripheralSendFtms();          // power=0, speed=0 megy ki (mert a stale védelem
        }                                     //   már nullázta az értékeket)
        lastFtmsNotify = now;
    }

    // ===================================================================
    // 7. CPS notify (Garmin óra)
    // ===================================================================
    if (now - lastCpsNotify >= 1000) {
        if (isCpsClientConnected()) {
            blePeripheralSendCps();
        }
        lastCpsNotify = now;
    }

    // ===================================================================
    // 8. CSC notify (telefon/app) – külön csatorna
    // ===================================================================
    if (now - lastCscNotify >= 1500) {
        if (isCscClientConnected()) {
            blePeripheralSendCsc();         // itt is a preferált (külső) cadence megy ki
        }
        lastCscNotify = now;
    }
}

// ================================================
// Segédfüggvények (más fájlokból is hívhatók)
// ================================================
bool isSchedulerStale() {
    return isStale;
}

uint32_t getReconnectInterval() {
    return reconnectInterval;
}
