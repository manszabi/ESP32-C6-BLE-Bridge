#include "ftms_parser.h"
#include "data_model.h"

// ================================================================
// FTMS Indoor Bike Data Flags (Section 4.9.1)
// Little-endian, 16 bit
//
// Bit 0:  More Data                (0 = Instantaneous Speed JELEN VAN)
// Bit 1:  Average Speed present
// Bit 2:  Instantaneous Cadence present
// Bit 3:  Average Cadence present
// Bit 4:  Total Distance present
// Bit 5:  Resistance Level present
// Bit 6:  Instantaneous Power present
// Bit 7:  Average Power present
// Bit 8:  Expended Energy present
// Bit 9:  Heart Rate present
// Bit 10: Metabolic Equivalent present
// Bit 11: Elapsed Time present
// Bit 12: Remaining Time present
// ================================================================

#define FLAG_MORE_DATA           (1 << 0)   // invertált! 0 = speed jelen van
#define FLAG_AVG_SPEED           (1 << 1)
#define FLAG_INST_CADENCE        (1 << 2)
#define FLAG_AVG_CADENCE         (1 << 3)
#define FLAG_TOTAL_DISTANCE      (1 << 4)
#define FLAG_RESISTANCE_LEVEL    (1 << 5)
#define FLAG_INST_POWER          (1 << 6)
#define FLAG_AVG_POWER           (1 << 7)
#define FLAG_EXPENDED_ENERGY     (1 << 8)
#define FLAG_HEART_RATE          (1 << 9)
#define FLAG_METABOLIC_EQUIV     (1 << 10)
#define FLAG_ELAPSED_TIME        (1 << 11)
#define FLAG_REMAINING_TIME      (1 << 12)

// Segédfüggvények a little-endian olvasáshoz
static inline uint16_t readU16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline int16_t readS16(const uint8_t* p) {
    return (int16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t readU24(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16));
}

void parseFtmsIndoorBikeData(const uint8_t* data, size_t len) {
    if (len < 2) return;  // minimum: flags mező

    size_t offset = 0;

    // 1. Flags (2 byte, little-endian)
    uint16_t flags = readU16(&data[offset]);
    offset += 2;

    // Változók az adatoknak
    float speed = 0.0f;
    int cadence = 0;
    int power = 0;

    // 2. Instantaneous Speed (km/h, resolution 0.01)
    //    JELEN VAN, ha bit 0 == 0 (invertált logika!)
    if (!(flags & FLAG_MORE_DATA)) {
        if (offset + 2 > len) return;
        uint16_t rawSpeed = readU16(&data[offset]);
        speed = rawSpeed / 100.0f;  // km/h
        offset += 2;
    }

    // 3. Average Speed (km/h, resolution 0.01)
    if (flags & FLAG_AVG_SPEED) {
        if (offset + 2 > len) return;
        // uint16_t avgSpeed = readU16(&data[offset]);
        offset += 2;  // kihagyjuk, nem kell nekünk
    }

    // 4. Instantaneous Cadence (rpm, resolution 0.5)
    if (flags & FLAG_INST_CADENCE) {
        if (offset + 2 > len) return;
        uint16_t rawCadence = readU16(&data[offset]);
        cadence = rawCadence / 2;  // 0.5 rpm resolution → rpm
        offset += 2;
    }

    // 5. Average Cadence (rpm, resolution 0.5)
    if (flags & FLAG_AVG_CADENCE) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // 6. Total Distance (m, 3 byte, uint24)
    if (flags & FLAG_TOTAL_DISTANCE) {
        if (offset + 3 > len) return;
        // uint32_t totalDist = readU24(&data[offset]);
        offset += 3;
    }

    // 7. Resistance Level (unitless, sint16)
    if (flags & FLAG_RESISTANCE_LEVEL) {
        if (offset + 2 > len) return;
        // int16_t resistance = readS16(&data[offset]);
        offset += 2;
    }

    // 8. Instantaneous Power (watts, sint16)
    if (flags & FLAG_INST_POWER) {
        if (offset + 2 > len) return;
        power = readS16(&data[offset]);
        offset += 2;
    }

    // 9. Average Power (watts, sint16)
    if (flags & FLAG_AVG_POWER) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // 10. Expended Energy (3 mező: total, per hour, per minute)
    if (flags & FLAG_EXPENDED_ENERGY) {
        if (offset + 5 > len) return;
        // uint16_t totalEnergy    = readU16(&data[offset]);
        // uint16_t energyPerHour  = readU16(&data[offset + 2]);
        // uint8_t  energyPerMin   = data[offset + 4];
        offset += 5;
    }

    // 11. Heart Rate (uint8)
    if (flags & FLAG_HEART_RATE) {
        if (offset + 1 > len) return;
        // uint8_t hr = data[offset];
        offset += 1;
    }

    // 12. Metabolic Equivalent (resolution 0.1)
    if (flags & FLAG_METABOLIC_EQUIV) {
        if (offset + 1 > len) return;
        offset += 1;
    }

    // 13. Elapsed Time (seconds, uint16)
    if (flags & FLAG_ELAPSED_TIME) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // 14. Remaining Time (seconds, uint16)
    if (flags & FLAG_REMAINING_TIME) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // Frissítjük a globális adatmodellt
    updateTrainerFromSuito(power, cadence, speed);
}