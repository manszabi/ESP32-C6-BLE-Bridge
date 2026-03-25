#pragma once
#include <Arduino.h>

// Egyszerű parser váz – Indoor Bike Data feldolgozására
// data: bejövő FTMS frame
// len: frame hossza
void parseFtmsIndoorBikeData(const uint8_t* data, size_t len);
