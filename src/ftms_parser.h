#pragma once
#include <Arduino.h>

// FTMS Indoor Bike Data (0x2AD2) parser
// A valós FTMS spec szerint dolgozza fel a frame-et
void parseFtmsIndoorBikeData(const uint8_t* data, size_t len);