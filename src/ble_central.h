#pragma once
#include <Arduino.h>

bool bleCentralInit();
bool bleCentralConnectToSuito();
void bleCentralLoop();
bool bleCentralIsConnected();

void sendResistanceCommandToSuito(int targetPower);
