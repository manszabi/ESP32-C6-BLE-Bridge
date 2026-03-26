#pragma once
#include <Arduino.h>

struct TrainerData {
    int power = 0;
    int cadence = 0;              // aktuálisan használt cadence
    float speed = 0.0f;

    // Források külön:
    int cadence_suito = 0;
    int cadence_external = 0;     // külső cadence szenzor

    int heartRate = 0;            // pulzus HRM-ből

    uint32_t timestamp = 0;       // utolsó Suito adat
    uint32_t hrTimestamp = 0;     // utolsó HRM adat
    uint32_t cadExtTimestamp = 0; // utolsó külső cadence adat
};

extern TrainerData g_trainerData;

void updateTrainerFromSuito(int power, int cadence, float speed);
void updateHeartRate(int hr);
void updateExternalCadence(int cadence);
void refreshDerivedFields();
