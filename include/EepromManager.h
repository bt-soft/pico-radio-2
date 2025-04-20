#ifndef __EEPROMMANAGER_H
#define __EEPROMMANAGER_H

// inspiration from https://github.com/bimac/EEPstore/blob/main/src/EEPstore.h

#include <CRC.h>
#include <EEPROM.h>

#include "defines.h"
#include "utils.h"

#ifndef EEPROM_SIZE
#define EEPROM_SIZE 4096  // Alapértelmezett 2K érték, de lehet 512-4096 között módosítani
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
        // De ha csak statikus metódusokat használunk, akkor ez a konstruktor nem is fut le.
        // Biztosítsuk, hogy EEPROM.begin() lefut a setup()-ban!
        // EEPROM.begin(EEPROM_SIZE); // Ezt inkább vedd ki innen.
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
        EepromManager<T> storage(dataRef);  // Ez csak a CRC számításhoz kell itt
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
            // A hibaüzenetet már a getIfValid kiírta.
            // Itt hívjuk a loadDefaults()-t, de azt a StoreBase-nek kellene, nem az EepromManagernek.
            // A StoreBase::load() hívja ezt, és annak kell kezelnie a loadDefaults()-t.
            // Viszont a default adatok mentését itt kell elindítani.
            DEBUG("[%s] EEPROM content invalid at address %d, saving defaults!\n", className, address);
            // A dataRef már tartalmazza a default értékeket (a StoreBase konstruktora vagy loadDefaults miatt)
            return save(dataRef, address, className);  // Elmentjük a defaultot és visszaadjuk az új CRC-t
        } else {
            DEBUG("[%s] EEPROM load OK from address %d\n", className, address);
            return loadedCrc;  // Visszaadjuk az érvényes, beolvasott CRC-t
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
        // Létrehozunk egy példányt a CRC számításhoz
        EepromManager<T> storage(dataRef);  // Ez kiszámolja a storage.crc-t

        DEBUG("[%s] Saving data to EEPROM at address %d (Size: %d B)...", className, address, sizeof(storage));

        // Lementjük az adatokat ÉS a crc-t az EEPROM-ba
        EEPROM.put(address, storage.data);
        EEPROM.put(address + sizeof(T), storage.crc);  // CRC mentése az adatok után

        if (EEPROM.commit()) {
            DEBUG(" OK (CRC: %d)\n", storage.crc);
            return storage.crc;  // Visszaadjuk a CRC-t siker esetén
        } else {
            DEBUG(" FAILED!\n");
            return 0;  // Hibát jelzünk 0-val
        }
    }
};

#endif  // __EEPROMMANAGER_H
