#include "control_logic.h"
#include "ble_central.h"

void handleControlFromClient(int targetPower) {
    sendResistanceCommandToSuito(targetPower);
}
