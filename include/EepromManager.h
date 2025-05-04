#ifndef __EEPROMMANAGER_H
#define __EEPROMMANAGER_H

// inspiration from https://github.com/bimac/EEPstore/blob/main/src/EEPstore.h

#include <CRC.h>
#include <EEPROM.h>

#include <type_traits>

// #include "DebugDataInspector.h" // Ezt eltávolítjuk
#include "defines.h"
#include "utils.h"

#ifndef EEPROM_SIZE
#define EEPROM_SIZE 2048  // Alapértelmezett 2K érték, de lehet 512-4096 között módosítani
#endif

/**
 * EEPROM kezelő osztály
 * pl.: a konfigurációs adatokat az EEPROM-ba menti és onnan tölti be
 */
template <class T>
class EepromManager {

   private:
    T data;        // Betöltött/Mentendő adatok
    uint16_t crc;  // Az adatok CRC ellenőrző összege

   public:
    /**
     * Konstruktor
     *
     * @tparam T az adat típusa
     * @param dataRef az adatok referenciája
     * @note A konstruktorban kiszámoljuk a CRC-t is
     *
     */
    EepromManager(const T &dataRef)
        : data(dataRef), crc(calcCRC16((uint8_t *)&data, sizeof(T))) {  // EEPROM.begin hívás csak egyszer kellene, pl. a setup()-ban, nem minden példányosításkor.
    }

    /**
     * @brief Inicializálja az EEPROM-ot a megadott mérettel.
     * Ezt a setup() elején kell meghívni egyszer.
     */
    inline static void init() {
        EEPROM.begin(EEPROM_SIZE);
        DEBUG("EEPROM initialized with size: %d\n", EEPROM_SIZE);
    }

    /**
     * Ha az adatok érvényesek, azokat beolvassa az EEPROM-ból
     *
     * @tparam T az adatok típusa
     * @param dataRef az adatok referenciája
     * @param valid valid a beolvasott adat?
     * @param address az EEPROM címe
     * @return adatok CRC16 ellenőrző összege
     */
    inline static uint16_t getIfValid(T &dataRef, bool &valid, const uint16_t address = 0, const char *className = "Unknown") {
        // EepromManager<T> storage(dataRef); // Erre itt nincs szükség
        T tempData;                         // Ideiglenes hely a beolvasáshoz
        uint16_t readCrc;

        // Adatok beolvasása
        EEPROM.get(address, tempData);
        // CRC beolvasása az adatok után
        EEPROM.get(address + sizeof(T), readCrc);

        // CRC ellenőrzés
        uint16_t calculatedCrc = calcCRC16((uint8_t *)&tempData, sizeof(T));
        valid = (readCrc == calculatedCrc);

        DEBUG("[%s] Checking EEPROM validity at address %d. Read CRC: %d, Calculated CRC: %d -> %s\n", className, address, readCrc, calculatedCrc, valid ? "Valid" : "INVALID");

        if (valid) {
            dataRef = tempData;  // Csak akkor másoljuk át, ha érvényes
            return readCrc;      // Visszaadjuk a beolvasott/érvényes CRC-t
        } else {
            // Ha érvénytelen, nem adjuk vissza a rossz CRC-t, inkább 0-t,
            // vagy hagyjuk, hogy a load kezelje a default mentést.
            // A load() hívja a save()-et, ami visszaadja az új CRC-t.
            return 0;  // Jelezzük, hogy a CRC érvénytelen volt.
        }
    }

    /**
     * EEPROM-ból Beolvasás
     *
     * Ha nem érvényesek, beállítja őket az alapértelmezett értékekre
     * @tparam T az adatok típusa
     * @param dataRef az adatok referenciája
     * @param address az EEPROM címe
     * @return adatok CRC16 ellenőrző összege
     */
    inline static uint16_t load(T &dataRef, const uint16_t address = 0, const char *className = "Unknown") {
        bool valid = false;
        uint16_t loadedCrc = getIfValid(dataRef, valid, address, className);

        if (!valid) {
            DEBUG("[%s] EEPROM content invalid at address %d, saving defaults!\n", className, address);
            return save(dataRef, address, className);
        } else {
            DEBUG("[%s] EEPROM load OK from address %d\n", className, address);

            return loadedCrc;
        }
    }

    /**
     * Az EEPROM-ba mentés
     *
     * @tparam T az adatok típusa
     * @param address az EEPROM címe
     * @param dataRef az adatok referenciája
     * @return adatok CRC16 ellenőrző összege
     */
    inline static uint16_t save(const T &dataRef, const uint16_t address = 0, const char *className = "Unknown") {
        EepromManager<T> storage(dataRef);

        DEBUG("[%s] Saving data to EEPROM at address %d (Size: %d B)...", className, address, sizeof(T));

        EEPROM.put(address, storage.data);
        EEPROM.put(address + sizeof(T), storage.crc);

        if (EEPROM.commit()) {
            DEBUG(" OK (CRC: %d)\n", storage.crc);
            return storage.crc;
        } else {
            DEBUG(" FAILED!\n");
            return 0;
        }
    }
};

#endif  // __EEPROMMANAGER_H
