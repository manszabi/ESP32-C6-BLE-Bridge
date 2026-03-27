#include <Arduino.h>
#include "config.h"
#include "ble_central.h"
#include "ble_peripheral.h"
#include "scheduler.h"

void setup() {
#if DEBUG_SERIAL
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32C6 Suito BLE Bridge starting...");
#endif

    bleCentralInit();

    blePeripheralInit();
    blePeripheralStartAdvertising();

    bleScanStart();

    schedulerInit();
}

void loop() {
    schedulerLoop();
}
