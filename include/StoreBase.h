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

    // Referencia az adattagra, ez az ős használja
    virtual T &r() = 0;

    /**
     * @brief Visszaadja a leszármazott osztály nevét a debug üzenetekhez.
     * Muszáj implementálni a leszármazottban.
     * @return const char* Az osztály neve.
     */
    virtual const char *getClassName() const = 0;

    /**
     * @brief Végrehajtja a mentést az EEPROM-ba.
     * A leszármazott felülírhatja, hogy megadja a helyes címet.
     * @return uint16_t A mentett adatok CRC-je, vagy 0 hiba esetén.
     */
    virtual uint16_t performSave() {
        // Alapértelmezett implementáció (pl. Config-hoz, ami nem írja felül)
        // Feltételezi a 0-s címet.
        return EepromManager<T>::save(r(), 0, getClassName());
    }

    /**
     * @brief Végrehajtja a betöltést az EEPROM-ból.
     * A leszármazott felülírhatja, hogy megadja a helyes címet.
     * @return uint16_t A betöltött adatok CRC-je.
     */
    virtual uint16_t performLoad() {
        // Alapértelmezett implementáció (pl. Config-hoz, ami nem írja felül)
        // Feltételezi a 0-s címet.
        return EepromManager<T>::load(r(), 0, getClassName());
    }

   public:
    /**
     * Tárolt adatok mentése
     */
    virtual void forceSave() {
        DEBUG("[%s] Forcing save...\n", getClassName());
        uint16_t savedCrc = performSave();
        if (savedCrc != 0) {
            lastCRC = savedCrc;
        }
    }

    /**
     * Tárolt adatok betöltése
     */
    virtual void load() {
        DEBUG("[%s] Loading...\n", getClassName());
        lastCRC = performLoad();
    }

    /**
     * Alapértelmezett adatok betöltése
     * Muszáj implementálni a leszármazottban
     */
    virtual void loadDefaults() = 0;

    /**
     * CRC ellenőrzés és mentés indítása, ha szükséges
     */
    virtual void checkSave() final {

        uint16_t currentCrc = calcCRC16((uint8_t *)&r(), sizeof(T));
        if (lastCRC != currentCrc) {
            DEBUG("[%s] CRC mismatch (RAM: %d != EEPROM/Last: %d). Saving...\n", getClassName(), currentCrc, lastCRC);
            uint16_t savedCrc = performSave();  // Hívjuk a virtuális mentést
            if (savedCrc != 0) {                // Feltételezzük, hogy 0=hiba
                lastCRC = savedCrc;
                // A sikeres mentési üzenetet most már az EepromManager::save írja ki
            } else {
                DEBUG("[%s] Save FAILED!\n", getClassName());
            }
        }
    }
};

#endif  //__STOREBASE_H
