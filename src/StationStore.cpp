#include "StationStore.h"

#include "EepromManager.h"  // Szükséges a save/load hívásokhoz

// Alapértelmezett (üres) listák definíciója
const FmStationList_t DEFAULT_FM_STATIONS = {};
const AmStationList_t DEFAULT_AM_STATIONS = {};

// --- FmStationStore Helper Implementációk ---

bool FmStationStore::addStation(const StationData& newStation) {
    if (data.count >= MAX_FM_STATIONS) {
        DEBUG("FM Memory full. Cannot add station.\n");
        return false;  // Memória megtelt
    }
    // Ellenőrzés, hogy létezik-e már (opcionális, de hasznos)
    if (findStation(newStation.frequency, newStation.bandIndex) != -1) {
        DEBUG("FM Station already exists (Freq: %d, Band: %d).\n", newStation.frequency, newStation.bandIndex);
        return false;  // Már létezik
    }

    data.stations[data.count] = newStation;  // Hozzáadás a tömb végére
    data.count++;
    DEBUG("FM Station added: %s\n", newStation.name);
    checkSave();  // Változás volt, mentsünk ha kell
    return true;
}

bool FmStationStore::updateStation(uint8_t index, const StationData& updatedStation) {
    if (index >= data.count) {
        DEBUG("Invalid index for FM station update: %d\n", index);
        return false;  // Érvénytelen index
    }
    data.stations[index] = updatedStation;
    DEBUG("FM Station updated at index %d: %s\n", index, updatedStation.name);
    checkSave();
    return true;
}

bool FmStationStore::deleteStation(uint8_t index) {
    if (index >= data.count) {
        DEBUG("Invalid index for FM station delete: %d\n", index);
        return false;  // Érvénytelen index
    }
    // Elemek eltolása a törölt helyére
    for (uint8_t i = index; i < data.count - 1; ++i) {
        data.stations[i] = data.stations[i + 1];
    }
    data.count--;
    // Opcionális: Az utolsó (most már felesleges) elem nullázása
    memset(&data.stations[data.count], 0, sizeof(StationData));
    DEBUG("FM Station deleted at index %d.\n", index);
    checkSave();
    return true;
}

int FmStationStore::findStation(uint16_t frequency, uint8_t bandIndex) {
    for (uint8_t i = 0; i < data.count; ++i) {
        if (data.stations[i].frequency == frequency && data.stations[i].bandIndex == bandIndex) {
            return i;  // Megtaláltuk
        }
    }
    return -1;  // Nem található
}

// --- AmStationStore Helper Implementációk (Hasonlóan az FM-hez) ---

bool AmStationStore::addStation(const StationData& newStation) {
    if (data.count >= MAX_AM_STATIONS) {
        DEBUG("AM Memory full. Cannot add station.\n");
        return false;
    }
    if (findStation(newStation.frequency, newStation.bandIndex) != -1) {
        DEBUG("AM Station already exists (Freq: %d, Band: %d).\n", newStation.frequency, newStation.bandIndex);
        return false;
    }
    data.stations[data.count] = newStation;
    data.count++;
    DEBUG("AM Station added: %s\n", newStation.name);
    checkSave();
    return true;
}

bool AmStationStore::updateStation(uint8_t index, const StationData& updatedStation) {
    if (index >= data.count) {
        DEBUG("Invalid index for AM station update: %d\n", index);
        return false;
    }
    data.stations[index] = updatedStation;
    DEBUG("AM Station updated at index %d: %s\n", index, updatedStation.name);
    checkSave();
    return true;
}

bool AmStationStore::deleteStation(uint8_t index) {
    if (index >= data.count) {
        DEBUG("Invalid index for AM station delete: %d\n", index);
        return false;
    }
    for (uint8_t i = index; i < data.count - 1; ++i) {
        data.stations[i] = data.stations[i + 1];
    }
    data.count--;
    memset(&data.stations[data.count], 0, sizeof(StationData));
    DEBUG("AM Station deleted at index %d.\n", index);
    checkSave();
    return true;
}

int AmStationStore::findStation(uint16_t frequency, uint8_t bandIndex) {
    for (uint8_t i = 0; i < data.count; ++i) {
        if (data.stations[i].frequency == frequency && data.stations[i].bandIndex == bandIndex) {
            return i;
        }
    }
    return -1;
}
