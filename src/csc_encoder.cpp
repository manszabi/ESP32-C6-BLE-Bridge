#include "csc_encoder.h"
#include "cps_encoder.h"
#include "data_model.h"
#include "config.h"

// Kumulatív wheel számláló (1/1024 sec egységben)
static uint32_t cumulativeWheelRevs = 0;
static uint16_t lastWheelEventTime  = 0;
static uint32_t lastWheelUpdateMs   = 0;

void cscUpdateWheelCounter() {
    uint32_t now = millis();
    float speedKmh = g_trainerData.speed;

    if (speedKmh < 0.1f || now <= lastWheelUpdateMs) {
        lastWheelUpdateMs = now;
        return;
    }

    uint32_t elapsedMs = now - lastWheelUpdateMs;
    float distMeters = (speedKmh / 3.6f) * (elapsedMs / 1000.0f);
    float revs = distMeters / WHEEL_CIRCUMFERENCE_M;

    if (revs > 0.005f) {
        cumulativeWheelRevs += (uint32_t)(revs + 0.5f);

        uint32_t ticks = (elapsedMs * 1024UL) / 1000UL;
        lastWheelEventTime = (lastWheelEventTime + (uint16_t)ticks) & 0xFFFF;
    }

    lastWheelUpdateMs = now;
}

size_t cscEncode(uint8_t* frame, size_t maxLen) {
    if (maxLen < 11) return 0;

    size_t offset = 0;

    // Flags: Wheel + Crank data present
    frame[offset++] = 0x03;

    // Cumulative Wheel Revolutions (uint32)
    frame[offset++] = cumulativeWheelRevs & 0xFF;
    frame[offset++] = (cumulativeWheelRevs >> 8) & 0xFF;
    frame[offset++] = (cumulativeWheelRevs >> 16) & 0xFF;
    frame[offset++] = (cumulativeWheelRevs >> 24) & 0xFF;

    // Last Wheel Event Time (1/1024 sec)
    frame[offset++] = lastWheelEventTime & 0xFF;
    frame[offset++] = (lastWheelEventTime >> 8) & 0xFF;

    // Cumulative Crank Revolutions (a CPS encoder-ből)
    uint16_t crankRevs = cpsGetCumulativeCrankRevs();
    frame[offset++] = crankRevs & 0xFF;
    frame[offset++] = (crankRevs >> 8) & 0xFF;

    // Last Crank Event Time (a CPS encoder-ből)
    uint16_t crankTime = cpsGetLastCrankEventTime();
    frame[offset++] = crankTime & 0xFF;
    frame[offset++] = (crankTime >> 8) & 0xFF;

    return offset;
}
