#include <Arduino.h>
#include "config.h"
#include "ble_central.h"
#include "ble_central_hrm.h"
#include "ble_central_cadence.h"
#include "ble_peripheral.h"
#include "ble_scan.h"
#include "scheduler.h"

void setup() {
#if DEBUG_SERIAL
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32C6 Suito BLE Bridge starting...");
#endif

    // Central oldali init
    bleCentralInit();         // NimBLE init + power
    bleCentralHrmInit();
    bleCentralCadenceInit();

    // Peripheral oldali init
    blePeripheralInit();
    blePeripheralStartAdvertising();

    // Unified scanner init + első scan indítása
    bleScanInit();
    bleScanStart();

    // Scheduler
    schedulerInit();
}

void loop() {
    schedulerLoop();
}
