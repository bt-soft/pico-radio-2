#ifndef __UTILS_H
#define __UTILS_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <cstddef>

#include "defines.h"

// -- -Utils---
namespace Utils {
/**
 * Biztonságos string másolás
 * @param dest cél string
 * @param src forrás string
 */
template <typename T, size_t N>
void safeStrCpy(T (&dest)[N], const T *src) {
    // A strncpy használata a karakterlánc másolásához
    strncpy(dest, src, N - 1);  // Csak N-1 karaktert másolunk, hogy ne lépjük túl a cél tömböt
    dest[N - 1] = '\0';         // Biztosítjuk, hogy a cél tömb nullával legyen lezárva
}

/**
 * Tömb elemei nullák?
 */
template <typename T, size_t N>
bool isZeroArray(T (&arr)[N]) {
    for (size_t i = 0; i < N; ++i) {
        if (arr[i] != 0) {
            return false;  // Ha bármelyik elem nem nulla, akkor false-t adunk vissza
        }
    }
    return true;  // Ha minden elem nulla, akkor true-t adunk vissza
}

/**
 * Várakozás a soros port megnyitására
 * @param pTft a TFT kijelző példánya
 */
void debugWaitForSerial(TFT_eSPI &tft);

//--- TFT ---
void tftTouchCalibrate(TFT_eSPI &tft, uint16_t (&calData)[5]);
void displayException(TFT_eSPI &tft, const char *msg);

//--- Beep ----
/**
 *  Pitty hangjelzés
 */
void beepInit();
void beepTick();
void beepError();

//--- Arrays
/**
 * Két tömb összemásolása egy harmadikba
 */
template <typename T>
void mergeArrays(const T *source1, uint8_t length1, const T *source2, uint8_t length2, T *destination, uint8_t &destinationLength) {
    destinationLength = 0;  // Alapértelmezett méret

    // Ha nincs egyik tömb sem, akkor nincs mit csinálni
    if ((!source1 || length1 == 0) && (!source2 || length2 == 0)) {
        return;
    }

    // Új méret beállítása
    destinationLength = length1 + length2;

    uint8_t index = 0;

    // Másolás az első tömbből (ha van)
    if (source1 && length1 > 0) {
        for (uint8_t i = 0; i < length1; i++) {
            destination[index++] = source1[i];
        }
    }

    // Másolás a második tömbből (ha van)
    if (source2 && length2 > 0) {
        for (uint8_t i = 0; i < length2; i++) {
            destination[index++] = source2[i];
        }
    }
}

}  // namespace Utils

#endif  // __UTILS_H
