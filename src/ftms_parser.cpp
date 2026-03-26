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

#define FLAG_MORE_DATA           (1u << 0)   // invertált! 0 = speed jelen van
#define FLAG_AVG_SPEED           (1u << 1)
#define FLAG_INST_CADENCE        (1u << 2)
#define FLAG_AVG_CADENCE         (1u << 3)
#define FLAG_TOTAL_DISTANCE      (1u << 4)
#define FLAG_RESISTANCE_LEVEL    (1u << 5)
#define FLAG_INST_POWER          (1u << 6)
#define FLAG_AVG_POWER           (1u << 7)
#define FLAG_EXPENDED_ENERGY     (1u << 8)
#define FLAG_HEART_RATE          (1u << 9)
#define FLAG_METABOLIC_EQUIV     (1u << 10)
#define FLAG_ELAPSED_TIME        (1u << 11)
#define FLAG_REMAINING_TIME      (1u << 12)

// Segédfüggvények a little-endian olvasáshoz
static inline uint16_t readU16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline int16_t readS16(const uint8_t* p) {
    return (int16_t)(p[0] | (p[1] << 8));
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

    // ====================== FONTOS MEZŐK (Zwift aktívan használja) ======================

    // Instantaneous Speed (km/h, 0.01 felbontás)
    if (!(flags & FLAG_MORE_DATA)) {
        if (offset + 2 > len) return;
        speed = readU16(&data[offset]) / 100.0f;
        offset += 2;
    }

    // Instantaneous Cadence (rpm, 0.5 felbontás)
    if (flags & FLAG_INST_CADENCE) {
        if (offset + 2 > len) return;
        cadence = readU16(&data[offset]) / 2;
        offset += 2;
    }

    // Instantaneous Power (watt) - a legfontosabb mező Zwift számára
    if (flags & FLAG_INST_POWER) {
        if (offset + 2 > len) return;
        power = readS16(&data[offset]);
        offset += 2;
    }

    // ====================== KIHAGYOTT MEZŐK (egy csoportban) ======================
    // Ezeket a Zwift nem igényli ehhez az adathoz, ezért egyszerűen átlépjük őket

    if (flags & FLAG_AVG_SPEED) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    if (flags & FLAG_AVG_CADENCE) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    if (flags & FLAG_TOTAL_DISTANCE) {
        if (offset + 3 > len) return;
        offset += 3;
    }

    if (flags & FLAG_RESISTANCE_LEVEL) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    if (flags & FLAG_AVG_POWER) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    if (flags & FLAG_EXPENDED_ENERGY) {
        if (offset + 5 > len) return;
        offset += 5;
    }

    if (flags & FLAG_HEART_RATE) {
        if (offset + 1 > len) return;
        offset += 1;
    }

    if (flags & FLAG_METABOLIC_EQUIV) {
        if (offset + 1 > len) return;
        offset += 1;
    }

    if (flags & FLAG_ELAPSED_TIME) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    if (flags & FLAG_REMAINING_TIME) {
        if (offset + 2 > len) return;
        offset += 2;
    }

    // ====================== Adatok frissítése ======================
    updateTrainerFromSuito(power, cadence, speed);
}