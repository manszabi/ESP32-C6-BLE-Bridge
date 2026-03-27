#pragma once
#include <Arduino.h>

struct TrainerData {
    int power = 0;
    int cadence = 0;
    float speed = 0.0f;
    uint32_t timestamp = 0;       // utolsó Suito adat
};

extern TrainerData g_trainerData;

void updateTrainerFromSuito(int power, int cadence, float speed);
