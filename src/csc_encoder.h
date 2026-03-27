#pragma once
#include <Arduino.h>

// Cycling Speed and Cadence Service (0x1816) encoder
// Kumulatív wheel számláló + CSC Measurement frame összeállítás

// Wheel counter frissítése (FreeRTOS timer hívja periodikusan)
void cscUpdateWheelCounter();

// CSC Measurement frame összeállítása
// Visszaadja a frame méretét
size_t cscEncode(uint8_t* frame, size_t maxLen);
