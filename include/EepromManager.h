#ifndef __EEPROMMANAGER_H
#define __EEPROMMANAGER_H

// inspiration from https://github.com/bimac/EEPstore/blob/main/src/EEPstore.h

#include <CRC.h>
#include <EEPROM.h>

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
    EepromManager(const T &dataRef) : data(dataRef), crc(calcCRC16((uint8_t *)&data, sizeof(T))) { EEPROM.begin(EEPROM_SIZE); }

    /**
     * Ha az adatok érvényesek, azokat beolvassa az EEPROM-ból
     *
     * @tparam T az adatok típusa
     * @param dataRef az adatok referenciája
     * @param valid valid a beolvasott adat?
     * @param address az EEPROM címe
     * @return adatok CRC16 ellenőrző összege
     */
    inline static uint16_t getIfValid(T &dataRef, bool &valid, const uint16_t address = 0) {

        // Lokális példány létrehozása, közben a crc is számítódik
        EepromManager<T> storage(dataRef);

        // Kiolvassuk az EEPROM-ból a lokális példányba, közben crc is számítódik
        EEPROM.get(address, storage);

        // Azonos a crc?
        valid = storage.crc == calcCRC16((uint8_t *)&storage.data, sizeof(T));

        // Ha valid, akkor beállítjuk a dataRef-et
        if (valid) {
            dataRef = storage.data;
        }

        return storage.crc;
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
    inline static uint16_t load(T &dataRef, const uint16_t address = 0) {

        // Kiolvassuk az adatokat az EEPROM-ból
        bool valid = false;
        uint16_t crc32 = getIfValid(dataRef, valid, address);

        // Ha NEM valid, akkor lementjük az EEPROM-ba a dataRef (mint default) értékeit
        if (!valid) {
            Utils::beepError();
            Utils::beepError();
            DEBUG("EEPROM content is invalid, save defaults!\n");
            Utils::beepError();
            Utils::beepError();
            save(dataRef, address);
        } else {
            DEBUG("EEPROM load OK\n");
        }

        return crc32;
    }

    /**
     * Az EEPROM-ba mentés
     *
     * @tparam T az adatok típusa
     * @param address az EEPROM címe
     * @param dataRef az adatok referenciája
     * @return adatok CRC16 ellenőrző összege
     */
    inline static uint16_t save(const T &dataRef, const uint16_t address = 0) {

        // Létrehozunk egy saját példányt, közben a crc is számítódik
        EepromManager<T> storage(dataRef);

        // Lementjük az adatokat + a crc-t az EEPROM-ba
        EEPROM.put(address, storage);
        EEPROM.commit();

        return storage.crc;
    }
};

#endif  // __EEPROMMANAGER_H
