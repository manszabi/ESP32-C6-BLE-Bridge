#include <Arduino.h>
#include "config.h"
#include "ble_central.h"
#include "ble_peripheral.h"
#include "data_model.h"

unsigned long lastNotifyTime = 0;

void setup() {
#if DEBUG_SERIAL
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32C6 Suito BLE Bridge starting...");
#endif

    bleCentralInit();
    blePeripheralInit();

    bleCentralConnectToSuito();
    blePeripheralStartAdvertising();
}

void loop() {
    bleCentralLoop();

    unsigned long now = millis();
    if (now - lastNotifyTime >= NOTIFY_INTERVAL_MS) {
        lastNotifyTime = now;
        blePeripheralLoop();
    }
}
