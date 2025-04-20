#ifndef __STATIONDATA_H
#define __STATIONDATA_H

#include <Arduino.h>

// Maximális állomásszámok
#define MAX_FM_STATIONS 20
#define MAX_AM_STATIONS 50  // AM/LW/SW/SSB/CW

// Maximális név hossz + null terminátor
#define MAX_STATION_NAME_LEN 15
#define STATION_NAME_BUFFER_SIZE (MAX_STATION_NAME_LEN + 1)

// EEPROM címek (Ellenőrizd, hogy ne ütközzenek a Config címmel és férjenek el!)
#define EEPROM_FM_STATIONS_ADDR 1024
#define EEPROM_AM_STATIONS_ADDR 2048  // Feltételezve, hogy az FM lista elfér 1024 bájtban

// Egyetlen állomás adatai
struct StationData {
    char name[STATION_NAME_BUFFER_SIZE];  // Állomás neve
    uint16_t frequency;                   // Frekvencia (kHz vagy 10kHz, a bandType alapján)
    uint8_t bandIndex;                    // A BandTable indexe, hogy tudjuk a sáv részleteit
    uint8_t modulation;                   // Aktuális moduláció (FM, AM, LSB, USB, CW)
    // Opcionálisan további adatok menthetők: bandwidth index, step index stb.
};

// FM állomások listája
struct FmStationList_t {
    StationData stations[MAX_FM_STATIONS];
    uint8_t count = 0;  // Tárolt állomások száma
};

// AM (és egyéb) állomások listája
struct AmStationList_t {
    StationData stations[MAX_AM_STATIONS];
    uint8_t count = 0;  // Tárolt állomások száma
};

#endif  // __STATIONDATA_H
