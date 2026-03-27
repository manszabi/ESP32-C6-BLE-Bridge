#pragma once
#include <Arduino.h>

// Cycling Power Service (0x1818) encoder
// Kumulatív crank számláló + CPS Measurement frame összeállítás

// Crank counter frissítése (FreeRTOS timer hívja periodikusan)
void cpsUpdateCrankCounter();

// CPS Measurement frame összeállítása
// Visszaadja a frame méretét
size_t cpsEncode(uint8_t* frame, size_t maxLen);

// Crank adatok lekérdezése (CSC encoder is használja)
uint16_t cpsGetCumulativeCrankRevs();
uint16_t cpsGetLastCrankEventTime();
