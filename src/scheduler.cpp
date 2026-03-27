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
static uint32_t lastTxLog = 0;
static uint32_t lastScanAttempt = 0;

// Adaptív reconnect
static uint32_t lastSuccessfulSuitoData = 0;
static uint32_t reconnectInterval = 1500;
static uint8_t  reconnectFailCount = 0;
static uint8_t  scanFailCount = 0;
static bool     isStale = false;
static bool     everConnected = false;

static const uint32_t SCAN_COOLDOWN_MS = 10000;  // 10s szünet scan-ek között

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

    // 1. Stale védelem — csak ha már volt valaha kapcsolat
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

    // 2. Sikeres adat érkezésének detektálása
    if (g_trainerData.timestamp > lastSuccessfulSuitoData) {
        lastSuccessfulSuitoData = g_trainerData.timestamp;
        reconnectFailCount = 0;
        reconnectInterval = 1500;
        everConnected = true;
    }

    // 3. Scan logika — cooldown-nal, ne spammeljen
    if (!bleScanFtmsFound() && !bleScanIsRunning()) {
        uint32_t scanCooldown = SCAN_COOLDOWN_MS;
        if (scanFailCount > 5) scanCooldown = 30000;   // 30s ha sokszor nem találta

        if (now - lastScanAttempt >= scanCooldown) {
            lastScanAttempt = now;
            if (bleScanStart()) {
                scanFailCount++;
#if DEBUG_SERIAL
                Serial.printf("[SCAN] Searching for FTMS trainer... (attempt #%d)\n", scanFailCount);
#endif
            }
        }
    }

    // 4. Reconnect logika — csak ha van cím és nem fut scan
    if (now - lastSuitoCheck >= reconnectInterval) {
        lastSuitoCheck = now;

        if (!bleScanIsRunning() && bleScanFtmsFound()) {
            if (!bleCentralIsConnected()) {
                if (bleCentralConnectToSuito()) {
                    reconnectFailCount = 0;
                    scanFailCount = 0;
                    reconnectInterval = 1500;
                    everConnected = true;
                    Serial.println("[CONN] Suito connected successfully");
                } else {
                    reconnectFailCount++;
                    if (reconnectFailCount > 5) {
                        reconnectInterval = 5000;
                    } else if (reconnectFailCount > 2) {
                        reconnectInterval = 3000;
                    } else {
                        reconnectInterval = 1500;
                    }
#if DEBUG_SERIAL
                    Serial.printf("[CONN] Suito reconnect failed #%d, next in %dms\n",
                                reconnectFailCount, reconnectInterval);
#endif
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

    // 7. Periodikus TX log (rate limited)
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
