#pragma once
#include <Arduino.h>

bool blePeripheralInit();
void blePeripheralStartAdvertising();

void blePeripheralSendFtms();
void blePeripheralSendCps();
void blePeripheralSendCsc();

bool isFtmsClientConnected();
bool isCpsClientConnected();
bool isCscClientConnected();
bool bleCentralConnectToSuito();
bool bleCentralInit();
bool bleCentralIsConnected();
void bleCentralLoop();

void sendResistanceCommandToSuito(int targetPower);