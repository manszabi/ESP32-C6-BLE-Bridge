#include "cps_encoder.h"
#include "data_model.h"

// Kumulatív crank számláló (1/1024 sec egységben)
static uint16_t cumulativeCrankRevs = 0;
static uint16_t lastCrankEventTime  = 0;
static uint32_t lastCrankUpdateMs   = 0;

void cpsUpdateCrankCounter() {
    uint32_t now = millis();
    int16_t cadence = g_trainerData.cadence;

    if (cadence <= 0 || now <= lastCrankUpdateMs) {
        lastCrankUpdateMs = now;
        return;
    }

    uint32_t elapsedMs = now - lastCrankUpdateMs;
    float revs = (cadence / 60.0f) * (elapsedMs / 1000.0f);

    if (revs > 0.005f) {
        cumulativeCrankRevs = (cumulativeCrankRevs + (uint16_t)(revs + 0.5f)) & 0xFFFF;

        uint32_t ticks = (elapsedMs * 1024UL) / 1000UL;
        lastCrankEventTime = (lastCrankEventTime + (uint16_t)ticks) & 0xFFFF;
    }

    lastCrankUpdateMs = now;
}

size_t cpsEncode(uint8_t* frame, size_t maxLen) {
    if (maxLen < 8) return 0;

    size_t offset = 0;

    // Flags: Crank Revolution Data present (bit 5)
    uint16_t flags = 0x0020;
    frame[offset++] = flags & 0xFF;
    frame[offset++] = (flags >> 8) & 0xFF;

    // Instantaneous Power (sint16, watts)
    int16_t power = (int16_t)g_trainerData.power;
    frame[offset++] = power & 0xFF;
    frame[offset++] = (power >> 8) & 0xFF;

    // Cumulative Crank Revolutions
    frame[offset++] = cumulativeCrankRevs & 0xFF;
    frame[offset++] = (cumulativeCrankRevs >> 8) & 0xFF;

    // Last Crank Event Time (1/1024 sec)
    frame[offset++] = lastCrankEventTime & 0xFF;
    frame[offset++] = (lastCrankEventTime >> 8) & 0xFF;

    return offset;
}

uint16_t cpsGetCumulativeCrankRevs() {
    return cumulativeCrankRevs;
}

uint16_t cpsGetLastCrankEventTime() {
    return lastCrankEventTime;
}
