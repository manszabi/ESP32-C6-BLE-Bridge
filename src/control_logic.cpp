#include "control_logic.h"
#include "ble_central.h"
#include "config.h"

void handleControlFromClient(int targetPower) {
#if DEBUG_SERIAL
    Serial.printf("[CTRL] Zwift target power: %dW\n", targetPower);
#endif
    sendResistanceCommandToSuito(targetPower);
}
