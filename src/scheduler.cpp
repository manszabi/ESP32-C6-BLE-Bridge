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
static uint32_t lastReconnectCheck = 0;
static uint32_t lastTxLog = 0;
static uint32_t lastScanAttempt = 0;

// Reconnect / stale
static uint32_t lastSuccessfulSuitoData = 0;
static uint8_t  reconnectFailCount = 0;
static uint8_t  scanFailCount = 0;
static bool     isStale = false;
static bool     everConnected = false;

static const uint32_t SCAN_COOLDOWN_MS      = 5000;  // 5s scan-ek között
static const uint32_t RECONNECT_INTERVAL_MS = 5000;  // 5s reconnect-ek között

// ════════════════════════════════════════════════
// Inicializálás
// ════════════════════════════════════════════════

void schedulerInit() {
    uint32_t now = millis();
    lastFtmsNotify = now;
    lastCpsNotify  = now;
    lastCscNotify  = now;
    lastReconnectCheck = now;
    lastSuccessfulSuitoData = now;

    reconnectFailCount = 0;
    scanFailCount = 0;
    isStale = false;
    everConnected = false;
    lastScanAttempt = 0;

    Serial.println("Scheduler initialized");
}

// ════════════════════════════════════════════════
// Fő scheduler ciklus
// ════════════════════════════════════════════════

void schedulerLoop() {
    uint32_t now = millis();

    // 1. onDisconnect esemény kezelése — azonnali rescan
    if (bleCentralWasDisconnected()) {
        // Ha többször egymás után disconnect, lehet hogy a cím elavult
        reconnectFailCount++;
        if (reconnectFailCount > 3) {
            bleScanReset();
            reconnectFailCount = 0;
#if DEBUG_SERIAL
            Serial.println("[SCHED] Too many disconnects, rescanning...");
#endif
        }
        lastReconnectCheck = now - RECONNECT_INTERVAL_MS;  // azonnali retry
    }

    // 2. Stale védelem — csak ha már volt valaha kapcsolat
    if (everConnected && now - lastSuccessfulSuitoData > 1800) {
        if (!isStale) {
            g_trainerData.power = 0;
            g_trainerData.cadence = 0;
            g_trainerData.speed = 0.0f;
            isStale = true;
#if DEBUG_SERIAL
            Serial.println("[STALE] Suito data timeout, values zeroed");
#endif
        }
    } else {
        isStale = false;
    }

    // 3. Sikeres adat érkezésének detektálása
    if (g_trainerData.timestamp > lastSuccessfulSuitoData) {
        lastSuccessfulSuitoData = g_trainerData.timestamp;
        reconnectFailCount = 0;
        everConnected = true;
    }

    // 4. Scan logika — 5s cooldown
    if (!bleScanFtmsFound() && !bleScanIsRunning()) {
        if (now - lastScanAttempt >= SCAN_COOLDOWN_MS) {
            lastScanAttempt = now;
            scanFailCount++;
            if (bleScanStart()) {
#if DEBUG_SERIAL
                Serial.printf("[SCAN] Searching for FTMS trainer... (attempt #%d)\n", scanFailCount);
#endif
            }
        }
    }

    // 5. Reconnect logika — csak ha van cím és nem fut scan
    if (now - lastReconnectCheck >= RECONNECT_INTERVAL_MS) {
        lastReconnectCheck = now;

        if (!bleScanIsRunning() && bleScanFtmsFound()) {
            if (!bleCentralIsConnected()) {
                if (bleCentralConnectToSuito()) {
                    reconnectFailCount = 0;
                    scanFailCount = 0;
                    everConnected = true;
                    Serial.println("[CONN] Suito connected successfully");
                } else {
                    reconnectFailCount++;
#if DEBUG_SERIAL
                    Serial.printf("[CONN] Reconnect failed #%d\n", reconnectFailCount);
#endif
                    // Ha sokszor nem sikerül, lehet elavult a cím
                    if (reconnectFailCount >= 5) {
                        bleScanReset();
                        reconnectFailCount = 0;
#if DEBUG_SERIAL
                        Serial.println("[SCHED] Address may be stale, rescanning...");
#endif
                    }
                }
            }
        }
    }

    // 6. FTMS notify Zwift felé (200 ms)
    if (now - lastFtmsNotify >= 200) {
        if (isFtmsClientConnected()) {
            blePeripheralSendFtms();
        }
        lastFtmsNotify = now;
    }

    // 7. CPS notify — Garmin (1000 ms)
    if (now - lastCpsNotify >= 1000) {
        if (isCpsClientConnected()) {
            blePeripheralSendCps();
        }
        lastCpsNotify = now;
    }

    // 8. CSC notify — Telefon/app (1000 ms)
    if (now - lastCscNotify >= 1000) {
        if (isCscClientConnected()) {
            blePeripheralSendCsc();
        }
        lastCscNotify = now;
    }

    // 9. Periodikus TX log (rate limited)
#if DEBUG_SERIAL
    if (now - lastTxLog >= LOG_RATE_LIMIT_MS) {
        lastTxLog = now;
        bool ftms = isFtmsClientConnected();
        bool cps  = isCpsClientConnected();
        bool csc  = isCscClientConnected();
        if (ftms || cps || csc) {
            Serial.printf("[TX] power=%dW cad=%drpm spd=%.1fkm/h -> %s%s%s\n",
                          g_trainerData.power,
                          g_trainerData.cadence,
                          g_trainerData.speed,
                          ftms ? "FTMS " : "",
                          cps  ? "CPS "  : "",
                          csc  ? "CSC"   : "");
        }
    }
#endif
}
