#ifndef __STATIONSTORE_H
#define __STATIONSTORE_H

#include "Band.h"  // Szükséges a BandTable eléréséhez
#include "StationData.h"
#include "StoreBase.h"

// Üres alapértelmezett listák deklarációja (definíció a .cpp fájlban)
extern const FmStationList_t DEFAULT_FM_STATIONS;
extern const AmStationList_t DEFAULT_AM_STATIONS;

// --- FM Station Store ---
class FmStationStore : public StoreBase<FmStationList_t> {
   public:
    FmStationList_t data;  // A tárolt FM állomások

   protected:
    FmStationList_t& r() override { return data; }

    const char* getClassName() const override { return "FM_Store"; }  // Rövidebb név

    // Felülírjuk a mentést/betöltést a helyes címmel és névvel
    uint16_t performSave() override { return EepromManager<FmStationList_t>::save(r(), EEPROM_FM_STATIONS_ADDR, getClassName()); }

    uint16_t performLoad() override {
        uint16_t loadedCrc = EepromManager<FmStationList_t>::load(r(), EEPROM_FM_STATIONS_ADDR, getClassName());
        // Count ellenőrzés marad itt
        if (data.count > MAX_FM_STATIONS) {
            DEBUG("[%s] Warning: FM station count corrected from %d to %d.\n", getClassName(), data.count, MAX_FM_STATIONS);
            data.count = MAX_FM_STATIONS;
            // Mivel módosítottuk az adatot, a betöltött CRC már nem érvényes a RAM tartalomra nézve.
            // Azonnal mentsük el a javított adatot, hogy a következő checkSave ne írja felül feleslegesen.
            // VAGY: A loadDefaults() hívása utáni mentés az EepromManager::load-ban ezt már kezeli.
            // Jobb, ha a loadDefaults utáni mentésre bízzuk.
        }
        return loadedCrc;  // Visszaadjuk a betöltött (vagy default mentés utáni) CRC-t
    }

   public:
    FmStationStore() : StoreBase<FmStationList_t>(), data(DEFAULT_FM_STATIONS) {}

    void loadDefaults() override {
        memcpy(&data, &DEFAULT_FM_STATIONS, sizeof(FmStationList_t));
        // Biztosítjuk, hogy a count is 0 legyen
        data.count = 0;
        DEBUG("FM Station defaults loaded.\n");
    }

    // Helper metódusok (implementáció a .cpp-ben)
    bool addStation(const StationData& newStation);
    bool updateStation(uint8_t index, const StationData& updatedStation);
    bool deleteStation(uint8_t index);
    int findStation(uint16_t frequency, uint8_t bandIndex);  // Visszaadja az indexet, vagy -1

    inline uint8_t getStationCount() const { return data.count; }

    inline const StationData* getStationByIndex(uint8_t index) const { return (index < data.count) ? &data.stations[index] : nullptr; }
};

// --- AM Station Store ---
class AmStationStore : public StoreBase<AmStationList_t> {
   public:
    AmStationList_t data;  // A tárolt AM/SW/LW/stb. állomások

   protected:
    AmStationList_t& r() override { return data; }

    const char* getClassName() const override { return "AM_Store"; }  // Rövidebb név

    // Felülírjuk a mentést/betöltést a helyes címmel és névvel
    uint16_t performSave() override { return EepromManager<AmStationList_t>::save(r(), EEPROM_AM_STATIONS_ADDR, getClassName()); }

    uint16_t performLoad() override {
        uint16_t loadedCrc = EepromManager<AmStationList_t>::load(r(), EEPROM_AM_STATIONS_ADDR, getClassName());
        // Count ellenőrzés marad itt
        if (data.count > MAX_AM_STATIONS) {
            DEBUG("[%s] Warning: AM station count corrected from %d to %d.\n", getClassName(), data.count, MAX_AM_STATIONS);
            data.count = MAX_AM_STATIONS;
        }
        return loadedCrc;
    }

   public:
    AmStationStore() : StoreBase<AmStationList_t>(), data(DEFAULT_AM_STATIONS) {}

    void loadDefaults() override {
        memcpy(&data, &DEFAULT_AM_STATIONS, sizeof(AmStationList_t));
        data.count = 0;
        DEBUG("AM Station defaults loaded.\n");
    }

        // Helper metódusok (hasonlóak az FM-hez)
    bool addStation(const StationData& newStation);
    bool updateStation(uint8_t index, const StationData& updatedStation);
    bool deleteStation(uint8_t index);
    int findStation(uint16_t frequency, uint8_t bandIndex);

    inline uint8_t getStationCount() const { return data.count; }

    inline const StationData* getStationByIndex(uint8_t index) const { return (index < data.count) ? &data.stations[index] : nullptr; }
};

// Globális példányok deklarációja (definíció a .cpp fájlban)
extern FmStationStore fmStationStore;
extern AmStationStore amStationStore;

#endif  // __STATIONSTORE_H
