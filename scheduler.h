#pragma once
#include <Arduino.h>

/**
 * ================================================
 * ESP32-C6 BLE Bridge Scheduler Header
 * 
 * Feladata: Időzíti a BLE műveleteket, kezeli az adaptív reconnect-et,
 *           és biztosítja a megfelelő sorrendet az adatfeldolgozásban.
 * 
 * Optimalizálva: 1 Hz-es Elite Suito + külső cadence szenzorhoz
 * ================================================
 */

// ======================
// Inicializálás és fő ciklus
// ======================

/**
 * Scheduler inicializálása
 * - Minden időzítőt alaphelyzetbe állít
 * - Beállítja a kezdeti reconnect paramétereket
 * - Csak egyszer hívandó a setup()-ban
 */
void schedulerInit();

/**
 * Fő scheduler ciklus
 * Minden Arduino loop()-ban meghívásra kerül.
 * 
 * Sorrend:
 *   1. Central adatfogadás
 *   2. Derived mezők frissítése (cadence prioritás)
 *   3. Stale védelem
 *   4. Adaptív reconnect logika
 *   5. Perifériás notify-k (FTMS, CPS, CSC)
 */
void schedulerLoop();

// ======================
// Időzítési változók (extern deklarációk)
// ======================

/**
 * Utolsó notify időpontok – a nem-blocking időzítéshez használjuk
 */
extern uint32_t lastFtmsNotify;     // FTMS (Zwift) notify utolsó ideje
extern uint32_t lastCpsNotify;      // CPS (Garmin) notify utolsó ideje
extern uint32_t lastCscNotify;      // CSC (telefon) notify utolsó ideje
extern uint32_t lastSuitoCheck;     // utolsó reconnect ellenőrzés ideje

// ======================
// Adaptív reconnect változók
// ======================

/**
 * Adaptív reconnect rendszer állapotváltozói
 * 
 * reconnectInterval:  jelenlegi ellenőrzési időköz (stabil: 1500 ms, probléma esetén gyorsabb)
 * reconnectFailCount: egymás utáni sikertelen reconnect-ek száma (backoff-hoz)
 * isStale:            true, ha a Suito adatai elavultak (power/speed nullázva)
 */
extern uint32_t reconnectInterval;   // aktuális reconnect intervallum (ms)
extern uint8_t  reconnectFailCount;  // hányszor sikertelen a reconnect egymás után
extern bool     isStale;             // stale állapot jelző

// ======================
// Segédfüggvények / állapot lekérdezők
// ======================

/**
 * Visszaadja, hogy jelenleg stale állapotban vagyunk-e
 * (hasznos más modulokban pl. debug vagy döntéshozatalhoz)
 */
bool isSchedulerStale();

/**
 * Visszaadja az aktuális reconnect intervallumot
 * (hasznos logginghoz vagy monitorozáshoz)
 */
uint32_t getReconnectInterval();
