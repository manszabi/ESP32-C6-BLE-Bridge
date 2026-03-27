#include "data_model.h"

TrainerData g_trainerData;

void updateTrainerFromSuito(int power, int cadence, float speed) {
    g_trainerData.power     = power;
    g_trainerData.cadence   = cadence;
    g_trainerData.speed     = speed;
    g_trainerData.timestamp = millis();
}
