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

   public:
    FmStationStore() : StoreBase<FmStationList_t>(), data(DEFAULT_FM_STATIONS) {}

    void loadDefaults() override {
        memcpy(&data, &DEFAULT_FM_STATIONS, sizeof(FmStationList_t));
        // Biztosítjuk, hogy a count is 0 legyen
        data.count = 0;
        DEBUG("FM Station defaults loaded.\n");
    }

    // Override save/load to specify address
    void forceSave() override {
        DEBUG("Forcing save FM stations to EEPROM...\n");
        EepromManager<FmStationList_t>::save(r(), EEPROM_FM_STATIONS_ADDR);
    }

    void load() override {
        DEBUG("Loading FM stations from EEPROM...\n");
        StoreBase<FmStationList_t>::lastCRC = EepromManager<FmStationList_t>::load(r(), EEPROM_FM_STATIONS_ADDR);
        // Biztosítjuk, hogy a count ne legyen nagyobb a maximumnál
        if (data.count > MAX_FM_STATIONS) {
            DEBUG("Warning: FM station count corrected from %d to %d.\n", data.count, MAX_FM_STATIONS);
            data.count = MAX_FM_STATIONS;
        }
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

   public:
    AmStationStore() : StoreBase<AmStationList_t>(), data(DEFAULT_AM_STATIONS) {}

    void loadDefaults() override {
        memcpy(&data, &DEFAULT_AM_STATIONS, sizeof(AmStationList_t));
        data.count = 0;
        DEBUG("AM Station defaults loaded.\n");
    }

    // Override save/load to specify address
    void forceSave() override {
        DEBUG("Forcing save AM stations to EEPROM...\n");
        EepromManager<AmStationList_t>::save(r(), EEPROM_AM_STATIONS_ADDR);
    }

    void load() override {
        DEBUG("Loading AM stations from EEPROM...\n");
        StoreBase<AmStationList_t>::lastCRC = EepromManager<AmStationList_t>::load(r(), EEPROM_AM_STATIONS_ADDR);
        if (data.count > MAX_AM_STATIONS) {
            DEBUG("Warning: AM station count corrected from %d to %d.\n", data.count, MAX_AM_STATIONS);
            data.count = MAX_AM_STATIONS;
        }
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
