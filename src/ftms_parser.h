#pragma once
#include <Arduino.h>

// FTMS Indoor Bike Data (0x2AD2) parser
void parseFtmsIndoorBikeData(const uint8_t* data, size_t len);
