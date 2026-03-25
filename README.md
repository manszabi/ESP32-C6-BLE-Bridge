# ESP32-C6-BLE-Bridge
Elite bridge
┌───────────────────────────────────────────────────────────────┐
│                         Elite Suito                          │
│                      (Smart trainer, BLE)                    │
│                                                               │
│  • Rádió + FW:                                                │
│     - BLE 4.x/5.x stack                                       │
│     - FTMS Service (UUID: 0x1826)                             │
│                                                               │
│  • FTMS Characteristics:                                      │
│     - Indoor Bike Data (notify)                              │
│     - Fitness Machine Control Point (write)                  │
│     - Fitness Machine Feature (read)                         │
│     - Supported Resistance Levels, Modes, stb.               │
│                                                               │
│  → Kimenet: Power, Cadence, Speed, Resistance control        │
└───────────────┬──────────────────────────────────────────────┘
                │ 1× BLE FTMS kapcsolat (Suito → ESP32‑C6)
                ▼
┌───────────────────────────────────────────────────────────────┐
│                         ESP32‑C6 Bridge                       │
│                 (Arduino + NimBLE / BLE stack)                │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  1. BLE Stack / Link Layer                              │  │
│  │     - BLE Controller (rádió)                            │  │
│  │     - BLE Host (GAP, GATT, L2CAP, SM)                   │  │
│  │     - Több kapcsolat kezelése (central + peripheral)    │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  2. Central Client modul (Suito felé)                   │  │
│  │     - Scan → Suito azonosítása (név / MAC / UUID)       │  │
│  │     - Csatlakozás FTMS Service‑hez                      │  │
│  │     - FTMS Indoor Bike Data characteristic subscribe    │  │
│  │     - Bejövő adatok:                                    │  │
│  │         • Power (W)                                     │  │
│  │         • Cadence (rpm)                                 │  │
│  │         • Speed (km/h)                                  │  │
│  │         • Distance, time (ha küldi)                     │  │
│  │     - Control Point write (ellenállás parancs továbbít) │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  3. Adatfeldolgozó / Normalizáló réteg                  │  │
│  │     - FTMS frame → belső struktúra:                     │  │
│  │         struct TrainerData {                            │  │
│  │            int power;                                   │  │
│  │            int cadence;                                 │  │
│  │            float speed;                                 │  │
│  │            uint32_t timestamp;                          │  │
│  │         };                                              │  │
│  │     - Egységek konverziója (m/s ↔ km/h, stb.)           │  │
│  │     - Bufferelés / smoothing (opcionális)               │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  4. Peripheral Server modulok (több szolgáltatás)       │  │
│  │                                                         │  │
│  │   4.1 FTMS Server (Zwift / TrainerRoad felé)            │  │
│  │       - FTMS Service (0x1826)                           │  │
│  │       - Indoor Bike Data (notify)                       │  │
│  │       - Control Point (write)                           │  │
│  │       - Zwift → Control Point → továbbítás Suitónak     │  │
│  │                                                         │  │
│  │   4.2 CPS – Cycling Power Service (Garmin óra felé)     │  │
│  │       - Service UUID: 0x1818                            │  │
│  │       - Power Measurement (0x2A63, notify)              │  │
│  │       - Power Feature (read)                            │  │
│  │       - Power adat TrainerData.power‑ből                │  │
│  │                                                         │  │
│  │   4.3 CSC – Cycling Speed and Cadence (Telefon felé)    │  │
│  │       - Service UUID: 0x1816                            │  │
│  │       - CSC Measurement (0x2A5B, notify)                │  │
│  │       - Wheel / crank revolution count + időbélyeg      │  │
│  │       - Speed/Cadence TrainerData‑ból számolva          │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  5. Kapcsolatkezelő / Scheduler                         │  │
│  │     - Connection intervalok beállítása (pl. 20 ms)      │  │
│  │     - Prioritás:                                        │  │
│  │         1) Suito FTMS (bejövő adat)                     │  │
│  │         2) Zwift FTMS (vezérlés + adat)                 │  │
│  │         3) CPS notify (Garmin)                          │  │
│  │         4) CSC notify (Telefon)                         │  │
│  │     - Időzítés: minden ciklusban:                       │  │
│  │         - új FTMS adat olvasása Suitótól                │  │
│  │         - TrainerData frissítése                        │  │
│  │         - notify küldése a csatlakozott klienseknek     │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  6. Vezérlés‑logika (Control Policy)                    │  │
│  │     - Csak 1 „master” vezérelheti a Suitót (pl. Zwift)  │  │
│  │     - Ha több eszköz próbál Control Pointot írni:       │  │
│  │         • csak a kijelölt master parancsait engeded     │  │
│  │         • a többit eldobod vagy hibával válaszolsz      │  │
│  │     - Opcionális: master lock (pl. első csatlakozó)     │  │
│  └─────────────────────────────────────────────────────────┘  │
└───────────────┬──────────────────────────────────────────────┘
                │ Több BLE kapcsolat (peripheral szerep)
                ▼
   ┌────────────────────────┬────────────────────────┬────────────────────────┐
   │        Zwift           │       Garmin óra       │        Telefon         │
   │   (FTMS Master app)    │   (CPS kliens)         │   (CSC kliens / app)   │
   │                        │                        │                        │
   │  - FTMS Service        │  - Power Measurement   │  - Speed/Cadence       │
   │  - vezérli az ERG‑et   │  - csak adat           │  - csak adat           │
   └────────────────────────┴────────────────────────┴────────────────────────┘

/esp32c6_ble_bridge/
│
├── src/
│   ├── main.cpp
│   ├── config.h
│   │
│   ├── ble_central.cpp
│   ├── ble_central.h
│   │
│   ├── ble_peripheral.cpp
│   ├── ble_peripheral.h
│   │
│   ├── data_model.cpp
│   ├── data_model.h
│   │
│   ├── ftms_parser.cpp
│   ├── ftms_parser.h
│   │
│   ├── cps_encoder.cpp
│   ├── cps_encoder.h
│   │
│   ├── csc_encoder.cpp
│   ├── csc_encoder.h
│   │
│   ├── control_logic.cpp
│   ├── control_logic.h
│   │
│   └── scheduler.cpp
│       scheduler.h
│
├── platformio.ini   (ha PlatformIO-t használsz)
└── README.md
