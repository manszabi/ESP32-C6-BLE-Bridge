#include "ble_central.h"
#include "config.h"
#include "ftms_parser.h"

#include <NimBLEDevice.h>

static NimBLEClient* suitoClient = nullptr;
static NimBLERemoteCharacteristic* ftmsIndoorBikeDataChar = nullptr;
static NimBLERemoteCharacteristic* ftmsControlPointChar   = nullptr;

// Indoor Bike Data notify callback
class FtmsNotifyCallback : public NimBLERemoteCharacteristicCallbacks {
    void onNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) override {
        // Itt jönnek a Suito FTMS adatai
        parseFtmsIndoorBikeData(data, len);
    }
};

bool bleCentralInit() {
    NimBLEDevice::init("ESP32C6-Bridge-Central");
    NimBLEDevice::setPower(ESP_PWR_LVL_P7);
    return true;
}

bool bleCentralConnectToSuito() {
    if (suitoClient && suitoClient->isConnected()) {
        return true;
    }

    suitoClient = NimBLEDevice::createClient();

    Serial.println("Connecting to Suito...");
    if (!suitoClient->connect(SUITO_MAC_ADDRESS)) {
        Serial.println("Failed to connect to Suito!");
        return false;
    }

    Serial.println("Connected to Suito, discovering services...");

    NimBLERemoteService* ftmsService = suitoClient->getService("1826"); // FTMS
    if (!ftmsService) {
        Serial.println("FTMS service not found on Suito!");
        return false;
    }

    // Indoor Bike Data (0x2AD2)
    ftmsIndoorBikeDataChar = ftmsService->getCharacteristic("2AD2");
    if (!ftmsIndoorBikeDataChar) {
        Serial.println("Indoor Bike Data characteristic not found!");
        return false;
    }

    // Control Point (0x2AD9)
    ftmsControlPointChar = ftmsService->getCharacteristic("2AD9");
    if (!ftmsControlPointChar) {
        Serial.println("Control Point characteristic not found!");
        // nem kritikus, de jó ha van
    }

    // Feliratkozás notify-ra
    if (ftmsIndoorBikeDataChar->canNotify()) {
        ftmsIndoorBikeDataChar->subscribe(true, new FtmsNotifyCallback());
        Serial.println("Subscribed to Indoor Bike Data notifications.");
    } else {
        Serial.println("Indoor Bike Data characteristic cannot notify!");
    }

    return true;
}

void bleCentralLoop() {
    // NimBLE callback-alapú, itt nem kell sokat csinálni.
    // Ha akarsz, ellenőrizheted a kapcsolatot, de a reconnect-et a scheduler intézi.
}

bool bleCentralIsConnected() {
    return suitoClient && suitoClient->isConnected();
}

void sendResistanceCommandToSuito(int targetPower) {
    if (!ftmsControlPointChar || !suitoClient || !suitoClient->isConnected()) {
        return;
    }

    // FTMS Control Point frame váz – valós implementáció később
    // Példa: set target power
    uint8_t frame[5] = {0};

    // Ez csak illusztráció, a valós FTMS CP parancs formátuma specifikus:
    // frame[0] = OPCODE_SET_TARGET_POWER;
    // frame[1..2] = targetPower, stb.

    frame[0] = 0x05; // pl. "set target power" opcode (nem valós érték!)
    frame[1] = targetPower & 0xFF;
    frame[2] = (targetPower >> 8) & 0xFF;

    ftmsControlPointChar->writeValue(frame, sizeof(frame), true);
}
