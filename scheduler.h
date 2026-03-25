#pragma once
#include <Arduino.h>

// Inicializálás
void schedulerInit();

// Fő ciklus
void schedulerLoop();

// Időzítési paraméterek
extern uint32_t lastFtmsNotify;
extern uint32_t lastCpsNotify;
extern uint32_t lastCscNotify;
extern uint32_t lastSuitoCheck;
