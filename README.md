# ESP32‑C6 BLE Bridge for Elite Suito  
**BLE‑only többkapcsolatos híd Zwift, Garmin és mobilappok számára**

Ez a projekt egy ESP32‑C6 alapú BLE‑bridge, amely lehetővé teszi, hogy az **Elite Suito** okosgörgő adatai több BLE‑eszköz felé is továbbításra kerüljenek, miközben a Suito továbbra is csak **egyetlen BLE FTMS kapcsolatot** engedélyez.

A bridge célja:

- Zwift / TrainerRoad → teljes FTMS vezérlés  
- Garmin óra → Cycling Power Service (CPS)  
- Telefon / app → Cycling Speed & Cadence (CSC)  
- Stabil, időzített BLE kapcsolatkezelés  
- Reconnect‑logika és stale‑data védelem  

---

## 🚴‍♂️ Miért van szükség a bridge‑re?

Az Elite Suito BLE‑n **csak egyetlen FTMS kapcsolatot** enged.  
Ez azt jelenti, hogy ha Zwift csatlakozik, akkor:

- Garmin óra nem tud BLE‑n adatot kapni  
- telefonos appok sem tudnak csatlakozni  
- több eszköz egyidejű használata lehetetlen  

Az ESP32‑C6 bridge ezt oldja meg:

- a Suitótól érkező adatokat **több BLE szolgáltatásként** hirdeti  
- a Zwift továbbra is vezérelheti a görgőt  
- a többi eszköz read‑only adatot kap  

---

## 🧱 Részletes blokkdiagram – BLE‑only, ESP32‑C6 bridge‑el

┌───────────────────────────────────────────────────────────────┐
│                         Elite Suito                          │
│                      (Smart trainer, BLE)                    │
│                                                               │
│  • BLE FTMS Service (0x1826)                                  │
│     - Indoor Bike Data (notify)                               │
│     - Fitness Machine Control Point (write)                   │
│                                                               │
│  → Kimenet: Power, Cadence, Speed, Resistance control         │
└───────────────┬──────────────────────────────────────────────┘
│ 1× BLE FTMS kapcsolat (Suito → ESP32‑C6)
▼
┌───────────────────────────────────────────────────────────────┐
│                         ESP32‑C6 Bridge                       │
│                 (Arduino + NimBLE stack)                      │
│                                                               │
│  1. BLE Central (Suito felé)                                  │
│     - FTMS service keresése                                   │
│     - Indoor Bike Data subscribe                               │
│     - Control Point write → Suito                              │
│                                                               │
│  2. Adatfeldolgozó réteg                                       │
│     - FTMS frame → TrainerData struct                          │
│     - smoothing / timestamp                                    │
│                                                               │
│  3. BLE Peripheral (több szolgáltatás)                         │
│     - FTMS Server → Zwift                                      │
│     - CPS Server  → Garmin óra                                 │
│     - CSC Server  → Telefon / app                              │
│                                                               │
│  4. Scheduler (prioritásos időzítés)                           │
│     - Suito ↔ Zwift: 50 ms                                     │
│     - Garmin CPS: 300 ms                                       │
│     - Telefon CSC: 300 ms                                      │
│                                                               │
│  5. Control Logic                                               │
│     - Csak 1 master vezérelhet (Zwift)                         │
│     - Ütközések kezelése                                       │
│                                                               │
│  6. Reconnect + Stale Data                                      │
│     - Suito reconnect loop                                      │
│     - 500 ms után: power/cadence/speed = 0                     │
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

---

## 📡 Kapcsolatkezelés és időzítés

A BLE időosztásos működésű, ezért a bridge explicit schedulert használ:

| Kapcsolat | Prioritás | Frissítés |
|----------|-----------|-----------|
| Suito → ESP32 (FTMS) | ⭐⭐⭐⭐⭐ | minden ciklus |
| ESP32 → Zwift (FTMS) | ⭐⭐⭐⭐ | 50 ms |
| ESP32 → Garmin (CPS) | ⭐⭐ | 300 ms |
| ESP32 → Telefon (CSC) | ⭐⭐ | 300 ms |

Ez biztosítja, hogy:

- az ERG vezérlés **nem laggol**,  
- a rádió **nem telítődik**,  
- a Garmin és a telefon **nem terheli túl** a rendszert.

---

## 🔄 Reconnect és Stale Data

A Suito kapcsolat megszakadhat.  
A bridge ezt így kezeli:

- reconnect loop 1–2 másodpercenként  
- ha 500 ms‑nál régebbi adat érkezett →  
  - power = 0  
  - cadence = 0  
  - speed = 0  
  - notify csak akkor megy ki, ha friss adat van  

Ez megakadályozza, hogy a Garmin vagy a telefon „beragadjon” egy régi értéken.

---

## 📂 Projektstruktúra

esp32c6_ble_bridge/
│
├── src/
│   ├── main.cpp
│   ├── config.h
│   ├── data_model.h
│   ├── data_model.cpp
│   ├── ble_central.h
│   ├── ble_central.cpp
│   ├── ble_peripheral.h
│   ├── ble_peripheral.cpp
│
└── platformio.ini

---

## 🔧 Következő lépések

- FTMS adatfeldolgozás implementálása  
- CPS / CSC encoder megírása  
- Scheduler és reconnect‑logika hozzáadása  

A projekt már most készen áll a további fejlesztésre.

