#ifndef __STATIONDATA_H
#define __STATIONDATA_H

#include <Arduino.h>

// Maximális állomásszámok
#define MAX_FM_STATIONS 20
#define MAX_AM_STATIONS 50  // AM/LW/SW/SSB/CW

// Maximális név hossz + null terminátor
#define MAX_STATION_NAME_LEN 15
#define STATION_NAME_BUFFER_SIZE (MAX_STATION_NAME_LEN + 1)

// EEPROM címek (Optimalizált elrendezés)
#define EEPROM_FM_STATIONS_ADDR 64   // <-- FM a Config után (Vége kb. 64 + 403 - 1 = 466)
#define EEPROM_AM_STATIONS_ADDR 480  // <-- AM az FM után (Vége kb. 480 + 1003 - 1 = 1482)

// Egyetlen állomás adatai
struct StationData {
    char name[STATION_NAME_BUFFER_SIZE];  // Állomás neve
    uint16_t frequency;                   // Frekvencia (kHz vagy 10kHz, a bandType alapján)
    int16_t bfoOffset;                    // BFO eltolás Hz-ben SSB/CW esetén (0 AM/FM esetén)
    uint8_t bandIndex;                    // A BandTable indexe, hogy tudjuk a sáv részleteit
    uint8_t modulation;                   // Aktuális moduláció (FM, AM, LSB, USB, CW)
    uint8_t bandwidthIndex;               // Index a Band::bandWidthFM/AM/SSB tömbökben
    // uint16_t antCap;     // Opcionális: Ha az antenna kapacitást is menteni akarod
    // int16_t bfoOffset;   // Opcionális: Ha a BFO eltolást is menteni akarod
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
