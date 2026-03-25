void schedulerLoop() {
    uint32_t now = millis();

    // 1. Kritikus: Suito → ESP32 (FTMS read)
    bleCentralLoop();

    // 2. Kritikus: Zwift → notify (FTMS)
    if (now - lastFtmsNotify >= 50) {
        sendFtmsNotifyToZwift();
        lastFtmsNotify = now;
    }

    // 3. Nem kritikus: Garmin CPS
    if (now - lastCpsNotify >= 300) {
        sendCpsNotifyToGarmin();
        lastCpsNotify = now;
    }

    // 4. Nem kritikus: Telefon CSC
    if (now - lastCscNotify >= 300) {
        sendCscNotifyToPhone();
        lastCscNotify = now;
    }
}
