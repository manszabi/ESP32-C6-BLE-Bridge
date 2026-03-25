#include "ftms_parser.h"
#include "data_model.h"

// Ez egy erősen leegyszerűsített parser váz.
// Később pontosíthatjuk a FTMS spec alapján.

void parseFtmsIndoorBikeData(const uint8_t* data, size_t len) {
    if (len < 6) {
        return; // túl rövid frame
    }

    // Példa: feltételezzük, hogy:
    // data[0..1] = power (W, signed vagy unsigned)
    // data[2]    = cadence (rpm)
    // data[3..4] = speed * 100 (m/s vagy km/h skálázva)
    // Ez csak váz – a valós FTMS mezők ennél összetettebbek.

    int16_t power = (int16_t)(data[0] | (data[1] << 8));
    uint8_t cadence = data[2];
    uint16_t speedRaw = (uint16_t)(data[3] | (data[4] << 8));

    // Tegyük fel, hogy speedRaw = sebesség * 100 (km/h)
    float speed = speedRaw / 100.0f;

    updateTrainerFromSuito(power, cadence, speed);
}
