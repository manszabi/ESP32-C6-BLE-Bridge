#pragma once
#include <Arduino.h>

// ======================
// Inicializálás és fő ciklus
// ======================
void schedulerInit();
void schedulerLoop();

// ======================
// Időzítési változók (extern deklarációk)
// ======================
extern uint32_t lastFtmsNotify;
extern uint32_t lastCpsNotify;
extern uint32_t lastCscNotify;
extern uint32_t lastSuitoCheck;

// ======================
// Adaptív reconnect változók
// ======================
extern uint32_t reconnectInterval;      // aktuális reconnect intervallum (ms)
extern uint8_t  reconnectFailCount;     // hányszor sikertelen a reconnect egymás után
extern bool     isStale;                // stale állapot jelző

// ======================
// Segédfüggvények / állapot lekérdezők (opcionális, de ajánlott)
// ======================
// Ha szeretnéd ezeket használni más fájlokban is

bool isSchedulerStale();
uint32_t getReconnectInterval();

#endif
