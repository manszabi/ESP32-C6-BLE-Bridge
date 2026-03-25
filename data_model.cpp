#include "data_model.h"

TrainerData g_trainerData;

void updateTrainerFromSuito(int power, int cadence, float speed) {
    g_trainerData.power        = power;
    g_trainerData.cadence_suito = cadence;
    g_trainerData.speed        = speed;
    g_trainerData.timestamp    = millis();
    refreshDerivedFields();
}

void updateHeartRate(int hr) {
    g_trainerData.heartRate   = hr;
    g_trainerData.hrTimestamp = millis();
}

void updateExternalCadence(int cadence) {
    g_trainerData.cadence_external = cadence;
    g_trainerData.cadExtTimestamp  = millis();
    refreshDerivedFields();
}

void refreshDerivedFields() {
    // Ha van friss külső cadence, azt használjuk
    uint32_t now = millis();
    bool extFresh = (now - g_trainerData.cadExtTimestamp) < 2000; // 2 s timeout

    if (extFresh && g_trainerData.cadence_external > 0) {
        g_trainerData.cadence = g_trainerData.cadence_external;
    } else {
        g_trainerData.cadence = g_trainerData.cadence_suito;
    }
}
