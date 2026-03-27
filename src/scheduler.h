#pragma once
#include <Arduino.h>

// Scheduler — időzíti a BLE műveleteket
// Reconnect, stale védelem, notify ütemezés

void schedulerInit();
void schedulerLoop();
