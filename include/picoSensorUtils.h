#ifndef __PICO_SENSOR_UTILS_H
#define __PICO_SENSOR_UTILS_H

#include <Arduino.h>

#include "defines.h"  // PIN_VBUS

namespace PicoSensorUtils {

// --- Konstansok ---
#define AD_RESOLUTION 12  // 12 bites az ADC
#define V_REFERENCE 3.3f
#define CONVERSION_FACTOR (1 << AD_RESOLUTION)

// Külső feszültségosztó ellenállásai a VBUS méréshez (A0-ra kötve)
#define R1 197.5f                       // Ellenállás VBUS és A0 között (kOhm)
#define R2 99.5f                        // Ellenállás A0 és GND között (kOhm)
#define DIVIDER_RATIO ((R1 + R2) / R2)  // Feszültségosztó aránya

/**
 * AD inicializálása
 */
inline void init() { analogReadResolution(AD_RESOLUTION); }

/**
 * ADC olvasás és VBUS feszültség kiszámítása külső osztóval
 * @return A VBUS mért feszültsége Voltban.
 */
inline float readVBus() {

    // ADC érték átalakítása feszültséggé
    float voltageOut = (analogRead(PIN_VBUS_INPUT) * V_REFERENCE) / CONVERSION_FACTOR;

    // Eredeti feszültség számítása a feszültségosztó alapján
    return voltageOut * DIVIDER_RATIO;
}

/**
 * Kiolvassa a processzor hőmérsékletét
 * @return processzor hőmérséklete Celsius fokban
 */
inline float readCoreTemperature() { return analogReadTemp(); }

};  // namespace PicoSensorUtils

#endif  // __PICO_SENSOR_UTILS_H
