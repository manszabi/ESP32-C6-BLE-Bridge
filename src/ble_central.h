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

void handleControlFromClient(int targetPower);