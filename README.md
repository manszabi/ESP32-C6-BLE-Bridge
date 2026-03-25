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

