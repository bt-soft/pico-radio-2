#ifndef __STOREBASE_H
#define __STOREBASE_H
#include <Arduino.h>

#include <list>

#include "EepromManager.h"
#include "defines.h"

/**
 * Generikus wrapper ős osztály a mentés és betöltés + CRC számítás funkciókhoz
 * A beburkolt objektum EEPROM-ban történő kezelését oldja meg
 */
template <typename T>
class StoreBase {

   protected:
    // A tárolt adatok CRC32 ellenőrző összege
    uint16_t lastCRC = 0;

    /**
     * Referencia az adattagra, ez az ős használja
     */
    virtual T &r() = 0;

   public:
    /**
     * Tárolt adatok mentése
     */
    virtual void forceSave() { EepromManager<T>::save(r()); }

    /**
     * Tárolt adatok betöltése
     */
    virtual void load() { lastCRC = EepromManager<T>::load(r()); }

    /**
     * Alapértelmezett adatok betöltése
     * Muszáj implementálni a leszármazottban
     */
    virtual void loadDefaults() = 0;

    /**
     * CRC ellenőrzés és mentés indítása, ha szükséges
     */
    virtual void checkSave() final {

        uint16_t crc = calcCRC16((uint8_t *)&r(), sizeof(T));
        if (lastCRC != crc) {
            crc = EepromManager<T>::save(r());
            lastCRC = crc;
            DEBUG("StoreBase::checkSave() -> Saving the config to the EEPROM is OK., crc = %d\n", crc);
        }
    }
};

#endif  //__STOREBASE_H
